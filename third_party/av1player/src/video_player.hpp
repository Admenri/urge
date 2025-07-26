#ifndef _UVPX_VIDEO_H_
#define _UVPX_VIDEO_H_

#include <math.h>
#include <stdio.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "webm/mkvparser/mkvparser.h"

#include "audio_decoder.hpp"
#include "buffer.hpp"
#include "frame_buffer.hpp"
#include "timer.hpp"

#include "dll_defines.hpp"
#include "error_codes.hpp"
#include "file_reader.hpp"
#include "object_pool.hpp"
#include "packet.hpp"
#include "packet_queue.hpp"

#include "player.hpp"

struct Dav1dContext;
struct Dav1dPicture;

namespace uvpx {

static const double NS_PER_S = 1e9;

class VideoPlayer {
 private:
  typedef mkvparser::Segment seg_t;

  struct VpxData {
    Dav1dContext* codec;
    Dav1dPicture* img;
    bool initialized;
  };

 private:
  bool m_initialized;

  OnAudioDataDecoded m_onAudioDataDecoded;
  void* m_onAudioDataDecodedUserPtr;
  OnVideoFinished m_onVideoFinished;
  void* m_onVideoFinishedUserPtr;

  void reset();
  void destroy();
  void updateYUVData(double time);

  // decoding
  VpxData m_decoderData;
  FileReader* m_reader;
  mkvparser::EBMLHeader m_header;
  std::unique_ptr<seg_t> m_segment;
  const mkvparser::Cluster* m_cluster;
  const mkvparser::Tracks* m_tracks;
  AudioDecoder* m_audioDecoder;
  Buffer<unsigned char>* m_decodeBuffer;
  FrameBuffer* m_frameBuffer;
  const mkvparser::BlockEntry* m_blockEntry;
  long m_audioTrackIdx;
  bool m_hasVideo;
  bool m_hasAudio;
  std::atomic<size_t> m_framesDecoded;
  Timer m_timer;

  std::atomic<double> m_playTime;

  // threading
  std::thread m_thread;
  std::thread m_videoThread;
  std::mutex m_updateMutex;
  std::atomic<bool> m_threadRunning;
  char m_threadErrorDesc[UVPX_THREAD_ERROR_DESC_SIZE];

  void startDecodingThread();
  void stopDecodingThread();
  void decodingThread();
  void videoDecodingThread();
  void threadError(int errorState, const char* format, ...);

  PacketQueue m_videoQueue;
  PacketQueue m_audioQueue;

  struct VideoPacketRawData {
    uint8_t* data;
    uint32_t size;
    double time;
  };

  ThreadSafeQueue<VideoPacketRawData> m_videoPacketQueue;

  Packet* demuxPacket();
  Packet* getPacket(Packet::Type type);
  void decodePacket(Packet* p);

  ObjectPool<Packet>* m_packetPool;

 public:
  enum class State {
    Uninitialized,
    Initialized,
    Buffering,
    Playing,
    Paused,
    Stopped,
    Finished
  };
  std::atomic<State> m_state;

  Player::VideoInfo m_info;

  Player::Config m_config;

 public:
  VideoPlayer(const Player::Config& cfg);
  ~VideoPlayer();

  Player::LoadResult load(SDL_IOStream* io, int audioTrack, bool preloadFile);
  bool update(float dt);

  Player::VideoInfo* info();
  FrameBuffer* frameBuffer();

  void setOnAudioData(OnAudioDataDecoded func, void* userPtr);
  void setOnVideoFinished(OnVideoFinished func, void* userPtr);

  double playTime();
  float duration();

  void play();
  void pause();
  void stop();
  bool isStopped();
  bool isPaused();
  bool isPlaying();
  bool isFinished();

  void addAudioData(float* values, size_t count);

  static const Player::Config& defaultConfig();
  bool readStats(Player::Statistics* dst);
};

}  // namespace uvpx

#endif  // _UVPX_VIDEO_H_
