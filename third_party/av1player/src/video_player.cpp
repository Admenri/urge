#include "video_player.hpp"

#include "timer.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ratio>

#define UVPX_USE_INTERNAL_TIMER

namespace uvpx {

VideoPlayer::VideoPlayer(const Player::Config& cfg)
    : m_initialized(false),
      m_onAudioDataDecoded(nullptr),
      m_onAudioDataDecodedUserPtr(nullptr),
      m_onVideoFinished(nullptr),

      m_reader(nullptr),
      m_cluster(nullptr),

      m_audioDecoder(nullptr),

      m_decodeBuffer(nullptr),
      m_frameBuffer(nullptr),

      m_blockEntry(nullptr),
      m_audioTrackIdx(0),
      m_hasVideo(false),
      m_hasAudio(false),

      m_framesDecoded(0),

      m_playTime(0.0),
      m_threadRunning(false),
      m_state(State::Uninitialized) {
  m_config = cfg;
  m_packetPool = new uvpx::ObjectPool<Packet>(1024 * 4);

  reset();
}

VideoPlayer::~VideoPlayer() {
  destroy();
}

/**
 * Loads the video file.
 * @param fileName file name relative to the file root
 * @param audioTrack which audio track to select
 */
Player::LoadResult VideoPlayer::load(SDL_IOStream* io,
                                     int audioTrack,
                                     bool preloadFile) {
  reset();

  // open file
  m_reader = new FileReader();
  if (m_reader->open(io, preloadFile)) {
    debugLog("Failed to open iostream video file.");
    return Player::LoadResult::FileNotExists;
  }

  long long pos = 0;

  long long ret = m_header.Parse(m_reader, pos);
  if (ret < 0) {
    debugLog("EBMLHeader::Parse() failed.");
    return Player::LoadResult::FailedParseHeader;
  }

  seg_t* pSegment_;

  ret = seg_t::CreateInstance(m_reader, pos, pSegment_);
  if (ret) {
    debugLog("Segment::CreateInstance() failed.");
    return Player::LoadResult::FailedCreateInstance;
  }

  m_segment.reset(pSegment_);

  ret = m_segment->Load();
  if (ret < 0) {
    debugLog("Segment::Load() failed.");
    return Player::LoadResult::FailedLoadSegment;
  }

  const mkvparser::SegmentInfo* const pSegmentInfo = m_segment->GetInfo();
  if (pSegmentInfo == NULL) {
    debugLog("Segment::GetInfo() failed.");
    return Player::LoadResult::FailedGetSegmentInfo;
  }

  m_info.duration = (float)(pSegmentInfo->GetDuration() / NS_PER_S);
  m_tracks = m_segment->GetTracks();

  unsigned long track_num = 0;
  const unsigned long num_tracks = m_tracks->GetTracksCount();
  int currentAudioTrack = 0;

  m_hasVideo = false;
  m_hasAudio = false;

  while (track_num != num_tracks) {
    const mkvparser::Track* const pTrack =
        m_tracks->GetTrackByIndex(track_num++);

    if (pTrack == NULL)
      continue;

    const long trackType = pTrack->GetType();
    const long trackNumber = pTrack->GetNumber();

    if (trackType == mkvparser::Track::kVideo) {
      const mkvparser::VideoTrack* const pVideoTrack =
          static_cast<const mkvparser::VideoTrack*>(pTrack);

      m_info.width = (int)pVideoTrack->GetWidth();
      m_info.height = (int)pVideoTrack->GetHeight();
      m_info.frameRate =
          1.0f / (float)(pVideoTrack->GetDefaultDuration() / NS_PER_S);
      m_info.decodeThreadsCount =
          std::min(m_config.decodeThreadsCount, getSystemThreadsCount());

      // configure codec
      const char* codecId = pVideoTrack->GetCodecId();

      Dav1dSettings aom_config;
      if (!strcmp(codecId, "V_AV1")) {
        dav1d_default_settings(&aom_config);
        aom_config.max_frame_delay = std::max(0, m_config.maxFrameDelay);
        aom_config.n_threads = m_info.decodeThreadsCount;
      } else {
        debugLog("Unsupported video codec: %s", codecId);
        return Player::LoadResult::UnsupportedVideoCodec;
      }

      // initialize decoder
      int open_result = dav1d_open(&m_decoderData.codec, &aom_config);
      if (open_result) {
        debugLog("Failed to initialize decoder (%d)", open_result);
        return Player::LoadResult::FailedInitializeVideoDecoder;
      }

      // alloc framebuffer
      m_frameBuffer = new FrameBuffer(this, m_info.width, m_info.height,
                                      std::max(32, m_config.frameBufferCount));

      m_decoderData.initialized = true;
      m_hasVideo = true;

      debugLog("vpx video found!");
    }

    if (trackType == mkvparser::Track::kAudio) {
      currentAudioTrack++;

      if (audioTrack == currentAudioTrack - 1) {
        m_audioTrackIdx = trackNumber;

        const mkvparser::AudioTrack* const pAudioTrack =
            static_cast<const mkvparser::AudioTrack*>(pTrack);

        // Don't leak audio decoders
        SafeDelete<AudioDecoder>(m_audioDecoder);

        m_audioDecoder = new AudioDecoder(this, m_config.audioDecodeBufferSize);
        m_audioDecoder->init();

        // parse Vorbis headers
        size_t size;
        unsigned char* data =
            (unsigned char*)pAudioTrack->GetCodecPrivate(size);

        if (!m_audioDecoder->initHeader(data, size)) {
          debugLog("Failed to decode audio header");
          SafeDelete<AudioDecoder>(m_audioDecoder);
        } else {
          if (!m_audioDecoder->postInit()) {
            debugLog("Failed to post-init audio decoder");
            SafeDelete<AudioDecoder>(m_audioDecoder);
          } else {
            m_info.audioChannels = m_audioDecoder->channels();
            m_info.audioFrequency = m_audioDecoder->rate();
            m_info.audioSamples =
                (int)((m_info.audioChannels * m_info.audioFrequency) *
                      m_info.duration);

            m_hasAudio = true;

            debugLog("vpx audio found!");
          }
        }
      }
    }
  }

  m_info.hasAudio = m_hasAudio;

  m_decodeBuffer = new Buffer<unsigned char>(m_config.videoDecodeBufferSize);

  m_state = State::Initialized;
  m_initialized = true;

  return Player::LoadResult::Success;
}

/**
 * Destroys the video.
 */
void VideoPlayer::destroy() {
  stopDecodingThread();

  m_videoQueue.destroy();
  m_audioQueue.destroy();

  SafeDelete<Buffer<unsigned char>>(m_decodeBuffer);

  SafeDelete<FileReader>(m_reader);

  SafeDelete<AudioDecoder>(m_audioDecoder);

  if (m_decoderData.initialized)
    dav1d_close(&m_decoderData.codec);

  SafeDelete<FrameBuffer>(m_frameBuffer);

  SafeDelete<ObjectPool<Packet>>(m_packetPool);
}

void VideoPlayer::reset() {
  memset(&m_decoderData, 0, sizeof(m_decoderData));
  memset(&m_info, 0, sizeof(m_info));
  memset(m_threadErrorDesc, 0, UVPX_THREAD_ERROR_DESC_SIZE);

  m_hasVideo = false;
  m_hasAudio = false;
}

/**
 * Updates the video.
 * Should be called each frame.
 * @param dt frame delta time (in seconds)
 */
bool VideoPlayer::update(float dt) {
  bool callOnFinishEvent = false;

  if (m_state == State::Playing)
    m_playTime = m_playTime + dt;

  m_updateMutex.lock();
  {
    if (m_state == State::Playing) {
      // thread error messages
      if (strlen(m_threadErrorDesc) > 0) {
        debugLog(m_threadErrorDesc);
        m_threadErrorDesc[0] = '\0';
      }

      if (playTime() >= duration()) {
        m_state = State::Finished;
        callOnFinishEvent = true;
      }
    }
  }
  m_updateMutex.unlock();

  if (callOnFinishEvent) {
    if (m_onVideoFinished != nullptr)
      m_onVideoFinished(m_onVideoFinishedUserPtr);
  }

  return true;
}

/**
 * Copies vpx image YUV data into allocated area, which will later be
 * directly copied into Unity textures.
 */
void VideoPlayer::updateYUVData(double time) {
  Frame* curYUV = m_frameBuffer->lockWrite(time);

  int plane;
  const int bytespp = 1;

  for (plane = 0; plane < 3; ++plane) {
    const unsigned char* buf =
        (const unsigned char*)m_decoderData.img.data[plane];
    const int stride = ((plane == 0) ? m_decoderData.img.stride[0]
                                     : m_decoderData.img.stride[1]);
    int h = m_decoderData.img.p.h;
    if (plane > 0)
      h /= 2;

    int y;
    curYUV->setDraw();
    curYUV->resize(plane, stride, h);
    auto* dst = curYUV->plane(plane);
    for (y = 0; y < h; ++y) {
      std::memcpy(dst, buf, stride * bytespp);
      buf += stride;
      dst += stride;
    }
  }

  m_frameBuffer->unlockWrite();
}

Player::VideoInfo* VideoPlayer::info() {
  return &m_info;
}

FrameBuffer* VideoPlayer::frameBuffer() {
  return m_frameBuffer;
}

/// Plays the video.
void VideoPlayer::play() {
  debugLog("playing video...");

  if (!m_initialized)
    return;

  if (m_state == State::Paused) {
    debugLog("play: resuming video...");
    m_timer.resume();
    m_state = State::Playing;
  } else {
    debugLog("play: buffering video...");

    if (!m_threadRunning.load())
      startDecodingThread();

    m_state = State::Buffering;
  }
}

/// Pauses the video.
void VideoPlayer::pause() {
  if (!m_initialized)
    return;

  m_state = State::Paused;
  m_timer.pause();
}

/// Stops the video.
void VideoPlayer::stop() {
  if (!m_initialized)
    return;

  stopDecodingThread();

  [[maybe_unused]] std::lock_guard<std::mutex> lock(m_updateMutex);

  if (m_state != State::Stopped) {
    m_state = State::Stopped;
    m_framesDecoded = 0;

    if (m_audioDecoder)
      m_audioDecoder->resetDecode();

    m_videoQueue.destroy();
    m_audioQueue.destroy();

    m_frameBuffer->reset();

    m_blockEntry = nullptr;
    m_cluster = nullptr;

    m_timer.stop();
    m_playTime = 0.0;

    startDecodingThread();
  }
}

/// Checks whether the video is paused.
bool VideoPlayer::isPaused() {
  return m_state == State::Paused;
}

/// Checks whether the video is playing.
bool VideoPlayer::isPlaying() {
  return m_state == State::Playing;
}

bool VideoPlayer::isFinished() {
  return m_state == State::Finished;
}

/// Checks whether the video is stopped.
bool VideoPlayer::isStopped() {
  return m_state == State::Stopped;
}

/// Returns video playing time (in seconds).
double VideoPlayer::playTime() {
#ifdef UVPX_USE_INTERNAL_TIMER
  return m_timer.elapsedSeconds();
#else
  return m_playTime;
#endif
}

/// Returns video duration (in seconds).
float VideoPlayer::duration() {
  return m_info.duration;
}

/**
 * Main decoding thread.
 */
void VideoPlayer::decodingThread() {
  debugLog("decodingThread: running...");

  m_cluster = m_segment->GetFirst();

  while (m_threadRunning.load()) {
    if (m_state != State::Playing && m_state != State::Stopped &&
        m_state != State::Buffering) {
      std::this_thread::yield();
      continue;
    }

    // try to decode Video packet(s)
    if (m_hasVideo) {
      Packet* videoPacket = getPacket(Packet::Type::Video);

      while (videoPacket && !m_frameBuffer->isFull()) {
        decodePacket(videoPacket);
        m_videoQueue.pop();
        videoPacket = getPacket(Packet::Type::Video);
      }
    }

    // try to decode Audio packet(s)
    if (m_hasAudio) {
      // how much audio seconds ahead should be decoded
      const double audioPreloadAdvance = 0.2;
      const double preloadTime = playTime() + audioPreloadAdvance;

      Packet* audioPacket = getPacket(Packet::Type::Audio);
      while (audioPacket && audioPacket->time() <= preloadTime) {
        decodePacket(audioPacket);
        m_audioQueue.pop();
        audioPacket = getPacket(Packet::Type::Audio);
      }
    }

    // if we're in buffering state and got there, all is buffered
    if (m_state == State::Buffering) {
      m_state = State::Playing;
      m_timer.start();
    }

    std::this_thread::yield();
  }
}

void VideoPlayer::videoDecodingThread() {
  debugLog("videoDecodingThread: running...");

  bool needFlush = false;
  bool needSeeking = false;

  while (m_threadRunning.load()) {
    if (m_state != State::Playing && m_state != State::Stopped &&
        m_state != State::Buffering) {
      std::this_thread::yield();
      continue;
    }

    // decode video frames
    if (m_videoPacketQueue.size() > 0) {
      VideoPacketRawData packet = m_videoPacketQueue.front();

      if (packet.time < playTime()) {
        free(packet.data);
        m_videoPacketQueue.pop();
        needFlush = true;
        continue;  // skip old packets
      }

      if (needFlush) {
        dav1d_flush(m_decoderData.codec);
        needFlush = false;
        needSeeking = true;
      }

      if (needSeeking) {
        Dav1dSequenceHeader seq_hdr;
        if (dav1d_parse_sequence_header(&seq_hdr, packet.data, packet.size)) {
          free(packet.data);
          m_videoPacketQueue.pop();
          continue;
        }
        needSeeking = false;
      }

      Dav1dData frame_data;
      uint8_t* buffer = dav1d_data_create(&frame_data, packet.size);
      if (!buffer) {
        threadError(UVPX_FAILED_TO_DECODE_FRAME, "Failed to load data.");
        return;
      }

      // Copy frame data to buffer
      memcpy(buffer, packet.data, packet.size);
      free(packet.data);
      m_videoPacketQueue.pop();

      int decode_result = 0;
      int fetch_result = 0;

      do {
        decode_result = dav1d_send_data(m_decoderData.codec, &frame_data);
        if (decode_result < 0 && decode_result != DAV1D_ERR(EAGAIN)) {
          threadError(UVPX_FAILED_TO_DECODE_FRAME, "Failed to send data (%d).",
                      decode_result);
          return;
        }

        fetch_result =
            dav1d_get_picture(m_decoderData.codec, &m_decoderData.img);
        if (!fetch_result) {
          if (m_decoderData.img.p.layout != DAV1D_PIXEL_LAYOUT_I420) {
            threadError(UVPX_UNSUPPORTED_IMAGE_FORMAT,
                        "Unsupported image format: %d",
                        m_decoderData.img.p.layout);
            return;
          }

          do {
            m_frameBuffer->update(playTime(), 1.0 / m_info.frameRate);
            std::this_thread::yield();
          } while (m_frameBuffer->isFull());

          updateYUVData(packet.time);
          m_framesDecoded++;

          dav1d_picture_unref(&m_decoderData.img);
        }

        // printf("time: %f, send: %d, fetch: %d\n", packet.time, decode_result,
        //        fetch_result);
      } while (decode_result != 0 && m_threadRunning.load());

      dav1d_data_unref(&frame_data);
    }

    std::this_thread::yield();
  }
}

/**
 * Called when some error occurs in main thread.
 */
void VideoPlayer::threadError(int errorState, const char* format, ...) {
  [[maybe_unused]] std::lock_guard<std::mutex> lock(m_updateMutex);

  char buffer[UVPX_THREAD_ERROR_DESC_SIZE];

  va_list arglist;
  va_start(arglist, format);
  vsprintf(buffer, format, arglist);
  va_end(arglist);

  strncpy(m_threadErrorDesc, buffer, UVPX_THREAD_ERROR_DESC_SIZE);
}

/**
 * Starts decoding thread.
 * @return decoding thread
 */
void VideoPlayer::startDecodingThread() {
  m_threadRunning.store(true);
  m_thread = std::thread(&VideoPlayer::decodingThread, this);
  m_videoThread = std::thread(&VideoPlayer::videoDecodingThread, this);
}

/// Stops decoding thread.
void VideoPlayer::stopDecodingThread() {
  if (m_threadRunning.load()) {
    m_threadRunning.store(false);
    m_thread.join();
    m_videoThread.join();
  }
}

/**
 * Called from AudioDecoder after PCM data has been decoded.
 * @param values decoded PCM data (-1 <-> 1)
 * @param count PCM data count
 */
void VideoPlayer::addAudioData(float* values, size_t count) {
  if (m_onAudioDataDecoded != nullptr)
    m_onAudioDataDecoded(m_onAudioDataDecodedUserPtr, values, count);
}

/// Sets OnAudioDataDecoded callback
void VideoPlayer::setOnAudioData(OnAudioDataDecoded func, void* userPtr) {
  m_onAudioDataDecoded = func;
  m_onAudioDataDecodedUserPtr = userPtr;
}

/// Sets OnVideoFinished callback
void VideoPlayer::setOnVideoFinished(OnVideoFinished func, void* userPtr) {
  m_onVideoFinished = func;
  m_onVideoFinishedUserPtr = userPtr;
}

/**
 * Returns next packet from stream.
 */
Packet* VideoPlayer::demuxPacket() {
  if (m_blockEntry && m_blockEntry->EOS()) {
    return nullptr;
  } else if (m_blockEntry == nullptr && m_cluster && !m_cluster->EOS()) {
    long status = m_cluster->GetFirst(m_blockEntry);
    if (status < 0) {
      threadError(-1, "Error parsing first block of cluster");
      return nullptr;
    }
  }

  Packet* ret = nullptr;

  while (m_cluster && m_blockEntry) {
    const mkvparser::Block* const pBlock = m_blockEntry->GetBlock();
    const long long trackNum = pBlock->GetTrackNumber();
    const unsigned long tn = static_cast<unsigned long>(trackNum);
    const mkvparser::Track* const pTrack = m_tracks->GetTrackByNumber(tn);
    const long long time_ns = pBlock->GetTime(m_cluster);
    // const long long discard_ns = pBlock->GetDiscardPadding();
    const double time_sec = (double(time_ns) / NS_PER_S);

    if (pTrack == nullptr) {
      threadError(-1, "Unknown block track type");
      return ret;
    } else if (pBlock) {
      switch (pTrack->GetType()) {
        case mkvparser::Track::kVideo: {
          ret = m_packetPool->GetNextWithoutInitializing();
          ret = new (ret) Packet(pBlock, Packet::Type::Video, time_sec);

          m_videoQueue.enqueue(ret);
          break;
        }

        case mkvparser::Track::kAudio: {
          if (trackNum == m_audioTrackIdx) {
            ret = m_packetPool->GetNextWithoutInitializing();
            ret = new (ret) Packet(pBlock, Packet::Type::Audio, time_sec);

            m_audioQueue.enqueue(ret);
          }
          break;
        }
      }
    }

    long status = m_cluster->GetNext(m_blockEntry, m_blockEntry);
    if (status < 0) {
      threadError(-1, "Error parsing next block of cluster");
      return ret;
    }

    if (m_blockEntry == nullptr || m_blockEntry->EOS()) {
      m_cluster = m_segment->GetNext(m_cluster);
    }

    if (ret)
      return ret;
  }

  return ret;
}

/**
 * Decodes packet automatically by its type.
 * Packet will be deallocated.
 */
void VideoPlayer::decodePacket(Packet* p) {
  const mkvparser::Block* pBlock = p->block();
  const int frameCount = pBlock->GetFrameCount();
  const long long discard_padding = pBlock->GetDiscardPadding();

  for (int i = 0; i < frameCount; ++i) {
    const mkvparser::Block::Frame& theFrame = pBlock->GetFrame(i);
    const long size = theFrame.len;

    unsigned char* data = m_decodeBuffer->get(size);
    theFrame.Read(m_reader, data);

    switch (p->type()) {
      case Packet::Type::Video: {
        VideoPacketRawData rawData;
        rawData.data = (uint8_t*)malloc(size);
        rawData.size = size;
        rawData.time = p->time();
        if (rawData.data)
          memcpy(rawData.data, data, size);

        m_videoPacketQueue.push(rawData);

        break;
      }

      case Packet::Type::Audio: {
        if (!m_audioDecoder)
          break;

        int32_t total_frames = 0;
        int aerr = m_audioDecoder->decode(data, size, 0, 0, 0, discard_padding,
                                          &total_frames);
        if (aerr) {
          threadError(UVPX_AUDIO_DECODE_ERROR, "Failed to decode audio (%d)",
                      aerr);
          return;
        }
      }

      default:
        break;
    }
  }

  // SafeDelete<Packet>(p);
  m_packetPool->DeleteWithoutDestroying(p);
}

/**
 * Returns current packet of given type.
 * Return nullptr when no packet of given type exists.
 */
Packet* VideoPlayer::getPacket(Packet::Type type) {
  Packet* p = nullptr;

  do {
    switch (type) {
      case Packet::Type::Audio:
        p = m_audioQueue.first();
        break;

      case Packet::Type::Video:
        p = m_videoQueue.first();
        break;
    }

    // when packet does not exists, demux new packet
    if (p == nullptr) {
      Packet* demux = demuxPacket();
      if (demux == nullptr)  // no more packets of given type
        return nullptr;
    }

  } while (p == nullptr);

  return p;
}

/**
 * Reads current playback statistics.
 */
bool VideoPlayer::readStats(Player::Statistics* dst) {
  if (!m_initialized)
    return false;

  dst->framesDecoded = (int)m_framesDecoded;

  if (m_decodeBuffer)
    dst->videoBufferSize = (int)m_decodeBuffer->size();

  if (m_audioDecoder)
    dst->audioBufferSize = (int)m_audioDecoder->bufferSize();

  return true;
}

}  // namespace uvpx
