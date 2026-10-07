// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/video_demuxer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "SDL3/SDL_iostream.h"
#include "mkvparser/mkvparser.h"

#include "core/exception.h"
#include "core/filesystem.h"
#include "core/logger.h"

namespace urge {
namespace {

constexpr int64_t kNanosecondsPerSecond = 1000000000;
constexpr uint32_t kOggSerialNumber = 0x65677275;
constexpr uint8_t kOggFlagContinuation = 0x01;
constexpr uint8_t kOggFlagFirstPage = 0x02;
constexpr uint8_t kOggFlagLastPage = 0x04;

class MkvFileReader : public mkvparser::IMkvReader {
 public:
  explicit MkvFileReader(SDL_IOStream* stream)
      : stream_(stream), length_(SDL_GetIOSize(stream)) {}

  ~MkvFileReader() override {
    if (stream_)
      SDL_CloseIO(stream_);
  }

  MkvFileReader(const MkvFileReader&) = delete;
  MkvFileReader& operator=(const MkvFileReader&) = delete;

  int Read(long long position, long length, unsigned char* buffer) override {
    if (!stream_ || position < 0 || length < 0)
      return -1;
    if (length == 0)
      return 0;
    if (position >= length_)
      return -1;
    if (SDL_SeekIO(stream_, position, SDL_IO_SEEK_SET) < 0)
      return -1;
    return SDL_ReadIO(stream_, buffer, static_cast<size_t>(length)) ==
                   static_cast<size_t>(length)
               ? 0
               : -1;
  }

  int Length(long long* total, long long* available) override {
    if (!stream_)
      return -1;
    if (total)
      *total = length_;
    if (available)
      *available = length_;
    return 0;
  }

  int64_t size() const { return length_; }

 private:
  SDL_IOStream* stream_ = nullptr;
  long long length_ = 0;
};

struct OggChecksumTable {
  uint32_t values[256];

  OggChecksumTable() {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t value = i << 24;
      for (int32_t bit = 0; bit < 8; ++bit)
        value = (value & 0x80000000u) ? ((value << 1) ^ 0x04c11db7u)
                                      : (value << 1);
      values[i] = value;
    }
  }
};

uint32_t OggChecksum(const uint8_t* data, size_t size) {
  static const OggChecksumTable table;
  uint32_t checksum = 0;
  for (size_t i = 0; i < size; ++i)
    checksum = (checksum << 8) ^
               table.values[((checksum >> 24) & 0xffu) ^ data[i]];
  return checksum;
}

void AppendLE32(std::vector<uint8_t>* out, uint32_t value) {
  for (int32_t i = 0; i < 4; ++i)
    out->push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xffu));
}

void AppendLE64(std::vector<uint8_t>* out, uint64_t value) {
  for (int32_t i = 0; i < 8; ++i)
    out->push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xffu));
}

class OggWriter {
 public:
  void WriteHeaders(const std::vector<std::vector<uint8_t>>& headers);
  void WritePacket(const uint8_t* data, size_t size, int64_t granule);
  void Finish(int64_t granule);

  std::vector<uint8_t> Take() { return std::move(bytes_); }

 private:
  void FlushPage(bool packet_continues);

  std::vector<uint8_t> bytes_;
  std::vector<uint8_t> lacing_;
  std::vector<uint8_t> payload_;
  uint32_t sequence_ = 0;
  int64_t granule_ = 0;
  bool first_ = true;
  bool continued_ = false;
  bool last_ = false;
};

void OggWriter::WritePacket(const uint8_t* data, size_t size, int64_t granule) {
  if (lacing_.size() == 255)
    FlushPage(false);

  size_t offset = 0;
  for (;;) {
    size_t chunk = size - offset;
    if (chunk > 255)
      chunk = 255;

    lacing_.push_back(static_cast<uint8_t>(chunk));
    payload_.insert(payload_.end(), data + offset, data + offset + chunk);
    offset += chunk;

    if (offset == size) {
      granule_ = granule;
      return;
    }

    if (lacing_.size() == 255)
      FlushPage(true);
  }
}

void OggWriter::FlushPage(bool packet_continues) {
  if (lacing_.empty())
    return;

  uint8_t flags = static_cast<uint8_t>((continued_ ? kOggFlagContinuation : 0) |
                                       (first_ ? kOggFlagFirstPage : 0));
  if (last_)
    flags |= kOggFlagLastPage;

  std::vector<uint8_t> page;
  page.reserve(27 + lacing_.size() + payload_.size());
  page.push_back('O');
  page.push_back('g');
  page.push_back('g');
  page.push_back('S');
  page.push_back(0);
  page.push_back(flags);
  AppendLE64(&page, static_cast<uint64_t>(packet_continues ? -1 : granule_));
  AppendLE32(&page, kOggSerialNumber);
  AppendLE32(&page, sequence_++);
  AppendLE32(&page, 0);
  page.push_back(static_cast<uint8_t>(lacing_.size()));
  page.insert(page.end(), lacing_.begin(), lacing_.end());
  page.insert(page.end(), payload_.begin(), payload_.end());

  const uint32_t checksum = OggChecksum(page.data(), page.size());
  for (int32_t i = 0; i < 4; ++i)
    page[22 + i] = static_cast<uint8_t>((checksum >> (8 * i)) & 0xffu);

  bytes_.insert(bytes_.end(), page.begin(), page.end());
  lacing_.clear();
  payload_.clear();
  first_ = false;
  continued_ = packet_continues;
}

void OggWriter::WriteHeaders(const std::vector<std::vector<uint8_t>>& headers) {
  WritePacket(headers.front().data(), headers.front().size(), 0);
  FlushPage(false);
  for (size_t i = 1; i < headers.size(); ++i)
    WritePacket(headers[i].data(), headers[i].size(), 0);
  FlushPage(false);
}

void OggWriter::Finish(int64_t granule) {
  granule_ = granule;
  last_ = true;
  FlushPage(false);
}

bool ParseXiphHeaders(const uint8_t* data,
                      size_t size,
                      std::vector<std::vector<uint8_t>>* headers) {
  if (!data || size < 2)
    return false;

  const size_t count = static_cast<size_t>(data[0]) + 1;
  size_t cursor = 1;
  std::vector<size_t> lengths(count, 0);
  for (size_t i = 0; i + 1 < count; ++i) {
    size_t length = 0;
    for (;;) {
      if (cursor >= size)
        return false;
      const uint8_t value = data[cursor++];
      length += value;
      if (value < 255)
        break;
    }
    lengths[i] = length;
  }

  size_t declared = 0;
  for (size_t i = 0; i + 1 < count; ++i)
    declared += lengths[i];
  if (cursor + declared >= size)
    return false;
  lengths[count - 1] = size - cursor - declared;

  headers->clear();
  headers->reserve(count);
  for (size_t i = 0; i < count; ++i) {
    headers->emplace_back(data + cursor, data + cursor + lengths[i]);
    cursor += lengths[i];
  }
  return true;
}

double ResolveFrameRate(const mkvparser::VideoTrack* track) {
  const double rate = track->GetFrameRate();
  if (rate > 0.0)
    return rate;

  const unsigned long long duration = track->GetDefaultDuration();
  if (duration > 0)
    return static_cast<double>(kNanosecondsPerSecond) /
           static_cast<double>(duration);
  return 30.0;
}

std::string ResolveCodecName(const char* name) { return name ? name : ""; }

}  // namespace

struct VideoDemuxer::Kernel {
  ~Kernel() {
    delete segment;
    delete reader;
  }

  MkvFileReader* reader = nullptr;
  mkvparser::EBMLHeader header;
  mkvparser::Segment* segment = nullptr;
  const mkvparser::Tracks* tracks = nullptr;
  const mkvparser::VideoTrack* video = nullptr;
  const mkvparser::AudioTrack* audio = nullptr;
  const mkvparser::Cluster* cluster = nullptr;
  const mkvparser::BlockEntry* entry = nullptr;
  bool at_cluster_start = true;
};

VideoDemuxer::VideoDemuxer() = default;

VideoDemuxer::~VideoDemuxer() = default;

bool VideoDemuxer::Open(const std::string& filename) {
  Close();

  SDL_IOStream* stream = nullptr;
  try {
    stream = IOService::Get().OpenReadRaw(filename);
  } catch (const Exception& error) {
    LOGGER_ERROR("the video '{}' could not be opened: {}", filename,
                 error.message());
    return false;
  }

  auto kernel = std::make_unique<Kernel>();
  kernel->reader = new MkvFileReader(stream);

  long long position = 0;
  if (kernel->header.Parse(kernel->reader, position) < 0) {
    LOGGER_ERROR("the video '{}' is not an EBML document", filename);
    return false;
  }

  mkvparser::Segment* segment = nullptr;
  if (mkvparser::Segment::CreateInstance(kernel->reader, position, segment) !=
          0 ||
      !segment) {
    LOGGER_ERROR("the video '{}' has no readable WebM segment", filename);
    return false;
  }
  kernel->segment = segment;

  if (segment->Load() < 0) {
    LOGGER_ERROR("the video '{}' could not be indexed", filename);
    return false;
  }

  const mkvparser::SegmentInfo* segment_info = segment->GetInfo();
  kernel->tracks = segment->GetTracks();
  if (!segment_info || !kernel->tracks) {
    LOGGER_ERROR("the video '{}' carries no track information", filename);
    return false;
  }

  info_.duration =
      static_cast<double>(segment_info->GetDuration()) / kNanosecondsPerSecond;
  info_.file_size = kernel->reader->size();

  const unsigned long track_count = kernel->tracks->GetTracksCount();
  for (unsigned long index = 0; index < track_count; ++index) {
    const mkvparser::Track* track = kernel->tracks->GetTrackByIndex(index);
    if (!track)
      continue;

    const char* codec = track->GetCodecId();
    if (!codec)
      continue;

    if (track->GetType() == mkvparser::Track::kVideo && !kernel->video) {
      if (std::strcmp(codec, "V_AV1") != 0)
        continue;

      const auto* video = static_cast<const mkvparser::VideoTrack*>(track);
      kernel->video = video;
      info_.width = static_cast<int32_t>(video->GetWidth());
      info_.height = static_cast<int32_t>(video->GetHeight());
      info_.frame_rate = ResolveFrameRate(video);
      info_.video_track = static_cast<int32_t>(video->GetNumber());
      info_.video_codec = codec;
      info_.video_codec_name = ResolveCodecName(video->GetCodecNameAsUTF8());
      info_.has_video = true;
    } else if (track->GetType() == mkvparser::Track::kAudio && !kernel->audio) {
      if (std::strcmp(codec, "A_VORBIS") != 0)
        continue;

      const auto* audio = static_cast<const mkvparser::AudioTrack*>(track);
      kernel->audio = audio;
      info_.audio_track = static_cast<int32_t>(audio->GetNumber());
      info_.audio_channels = static_cast<int32_t>(audio->GetChannels());
      info_.audio_sample_rate =
          static_cast<int32_t>(audio->GetSamplingRate());
      info_.audio_codec = codec;
      info_.audio_codec_name = ResolveCodecName(audio->GetCodecNameAsUTF8());
      info_.has_audio = true;
    }
  }

  if (!info_.has_video) {
    LOGGER_ERROR("the video '{}' has no AV1 video track", filename);
    return false;
  }

  kernel_ = std::move(kernel);
  ResetVideoCursor();
  return true;
}

void VideoDemuxer::Close() {
  kernel_.reset();
  info_ = VideoTrackInfo();
}

void VideoDemuxer::ResetVideoCursor() {
  if (!kernel_ || !kernel_->segment)
    return;

  kernel_->cluster = kernel_->segment->GetFirst();
  kernel_->entry = nullptr;
  kernel_->at_cluster_start = true;
}

std::vector<uint8_t> VideoDemuxer::ExtractAudio() {
  if (!kernel_ || !kernel_->audio || !kernel_->segment)
    return {};

  size_t private_size = 0;
  const unsigned char* private_data =
      kernel_->audio->GetCodecPrivate(private_size);
  std::vector<std::vector<uint8_t>> headers;
  if (!ParseXiphHeaders(private_data, private_size, &headers) ||
      headers.size() < 3) {
    LOGGER_WARN("the vorbis track of this video has no readable headers");
    return {};
  }

  const int64_t track_number = kernel_->audio->GetNumber();
  const double sample_rate = static_cast<double>(info_.audio_sample_rate);

  OggWriter writer;
  writer.WriteHeaders(headers);

  std::vector<uint8_t> packet;
  int64_t granule = 0;
  const mkvparser::Cluster* cluster = kernel_->segment->GetFirst();
  while (cluster && !cluster->EOS()) {
    const mkvparser::BlockEntry* entry = nullptr;
    if (cluster->GetFirst(entry) < 0)
      break;

    while (entry && !entry->EOS()) {
      const mkvparser::Block* block = entry->GetBlock();
      if (block && block->GetTrackNumber() == track_number &&
          block->GetFrameCount() > 0) {
        packet.clear();
        for (int32_t i = 0; i < block->GetFrameCount(); ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);
          const size_t offset = packet.size();
          packet.resize(offset + static_cast<size_t>(frame.len));
          if (frame.Read(kernel_->reader, packet.data() + offset) != 0) {
            LOGGER_WARN("a vorbis packet of this video could not be read");
            return {};
          }
        }

        if (sample_rate > 0.0) {
          granule = static_cast<int64_t>(std::llround(
              static_cast<double>(block->GetTime(cluster)) * sample_rate /
              static_cast<double>(kNanosecondsPerSecond)));
        }
        writer.WritePacket(packet.data(), packet.size(), granule);
      }

      if (cluster->GetNext(entry, entry) < 0) {
        entry = nullptr;
        break;
      }
    }

    cluster = kernel_->segment->GetNext(cluster);
  }

  if (sample_rate > 0.0)
    granule = std::max(granule, static_cast<int64_t>(std::llround(
                                    info_.duration * sample_rate)));
  writer.Finish(granule);
  return writer.Take();
}

bool VideoDemuxer::NextVideoPacket(VideoPacket* packet) {
  if (!kernel_ || !kernel_->video || !kernel_->segment || !packet)
    return false;

  const int64_t track_number = kernel_->video->GetNumber();
  for (;;) {
    if (!kernel_->cluster || kernel_->cluster->EOS())
      return false;

    if (kernel_->at_cluster_start) {
      if (kernel_->cluster->GetFirst(kernel_->entry) < 0)
        return false;
      kernel_->at_cluster_start = false;
    }

    if (!kernel_->entry || kernel_->entry->EOS()) {
      kernel_->cluster = kernel_->segment->GetNext(kernel_->cluster);
      kernel_->at_cluster_start = true;
      continue;
    }

    const mkvparser::Block* block = kernel_->entry->GetBlock();
    if (!block || block->GetTrackNumber() != track_number ||
        block->GetFrameCount() <= 0) {
      if (kernel_->cluster->GetNext(kernel_->entry, kernel_->entry) < 0)
        kernel_->entry = nullptr;
      continue;
    }

    packet->data.clear();
    for (int32_t i = 0; i < block->GetFrameCount(); ++i) {
      const mkvparser::Block::Frame& frame = block->GetFrame(i);
      const size_t offset = packet->data.size();
      packet->data.resize(offset + static_cast<size_t>(frame.len));
      if (frame.Read(kernel_->reader, packet->data.data() + offset) != 0) {
        LOGGER_ERROR("a video packet could not be read from the file");
        return false;
      }
    }

    packet->pts = static_cast<double>(block->GetTime(kernel_->cluster)) /
                  static_cast<double>(kNanosecondsPerSecond);
    packet->key = block->IsKey();

    if (kernel_->cluster->GetNext(kernel_->entry, kernel_->entry) < 0)
      kernel_->entry = nullptr;
    return true;
  }
}

bool VideoDemuxer::Seek(double seconds) {
  if (!kernel_ || !kernel_->video || !kernel_->segment)
    return false;

  const int64_t time = static_cast<int64_t>(
      std::max(0.0, seconds) * static_cast<double>(kNanosecondsPerSecond));

  const mkvparser::BlockEntry* entry = nullptr;
  if (kernel_->video->Seek(time, entry) < 0 || !entry || entry->EOS()) {
    ResetVideoCursor();
    return true;
  }

  kernel_->cluster = entry->GetCluster();
  kernel_->entry = entry;
  kernel_->at_cluster_start = false;
  return true;
}

}  // namespace urge
