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

#include "core/audio/decoders/backend.h"
#include <algorithm>
#include <limits>
#include "core/audio/decoders/decoder.h"
#include "core/common/logger.h"

namespace urge::audioformat {
namespace {
struct Source {
  ma_data_source_base base{};
  std::unique_ptr<Decoder> decoder;
  bool midi = false, warned = false;
};
ma_result Read(ma_data_source* data,
               void* output,
               ma_uint64 count,
               ma_uint64* read) {
  auto& source = *static_cast<Source*>(data);
  size_t wanted = static_cast<size_t>(std::min<ma_uint64>(
      count, std::numeric_limits<size_t>::max() / source.decoder->Channels()));
  size_t done = source.decoder->Read(static_cast<float*>(output), wanted);
  if (read)
    *read = done;
  if (source.decoder->DamagedFrames() && !source.warned) {
    LOGGER_WARN("[Audio] damaged WMA frames replaced with silence");
    source.warned = true;
  }
  return done ? MA_SUCCESS : MA_AT_END;
}
ma_result Seek(ma_data_source* data, ma_uint64 frame) {
  return static_cast<Source*>(data)->decoder->Seek(frame) ? MA_SUCCESS
                                                          : MA_INVALID_ARGS;
}
ma_result Format(ma_data_source* data,
                 ma_format* format,
                 ma_uint32* channels,
                 ma_uint32* rate,
                 ma_channel* map,
                 size_t capacity) {
  auto& decoder = *static_cast<Source*>(data)->decoder;
  if (format)
    *format = ma_format_f32;
  if (channels)
    *channels = decoder.Channels();
  if (rate)
    *rate = decoder.Rate();
  if (map && capacity >= decoder.Channels()) {
    map[0] = decoder.Channels() == 1 ? MA_CHANNEL_MONO : MA_CHANNEL_FRONT_LEFT;
    if (decoder.Channels() == 2)
      map[1] = MA_CHANNEL_FRONT_RIGHT;
  }
  return MA_SUCCESS;
}
ma_result Cursor(ma_data_source* data, ma_uint64* frame) {
  if (!frame)
    return MA_INVALID_ARGS;
  *frame = static_cast<Source*>(data)->decoder->Cursor();
  return MA_SUCCESS;
}
ma_result Length(ma_data_source* data, ma_uint64* frame) {
  if (!frame)
    return MA_INVALID_ARGS;
  *frame = static_cast<Source*>(data)->decoder->Length();
  return MA_SUCCESS;
}
ma_result Looping(ma_data_source*, ma_bool32) {
  return MA_SUCCESS;
}
const ma_data_source_vtable kSource = {Read,   Seek,    Format, Cursor,
                                       Length, Looping, 0};
ma_result Init(void*,
               ma_read_proc,
               ma_seek_proc,
               ma_tell_proc,
               void*,
               const ma_decoding_backend_config*,
               const ma_allocation_callbacks*,
               ma_data_source**) {
  return MA_NOT_IMPLEMENTED;
}
ma_result InitMemory(void* user,
                     const void* bytes,
                     size_t size,
                     const ma_decoding_backend_config*,
                     const ma_allocation_callbacks*,
                     ma_data_source** result) {
  if (!bytes || !result)
    return MA_INVALID_ARGS;
  std::span<const uint8_t> input(static_cast<const uint8_t*>(bytes), size);
  bool midi = IsMIDI(input);
  if (!midi && !IsWMA(input))
    return MA_INVALID_FILE;
  std::unique_ptr<Source> source(new Source);
  std::string error;
  if (midi) {
    std::shared_ptr<const SoundBank> bank;
    if (user)
      bank = *static_cast<std::shared_ptr<const SoundBank>*>(user);
    source->decoder = OpenMIDI(input, std::move(bank), error);
  } else {
    source->decoder = OpenWMA(input, error);
  }
  if (!source->decoder) {
    LOGGER_WARN("[Audio] {}", error);
    return MA_INVALID_FILE;
  }
  auto config = ma_data_source_config_init();
  config.vtable = &kSource;
  ma_result status = ma_data_source_init(&config, &source->base);
  if (status != MA_SUCCESS)
    return status;
  source->midi = midi;
  *result = source.release();
  return MA_SUCCESS;
}
void Uninit(void*, ma_data_source* data, const ma_allocation_callbacks*) {
  auto* source = static_cast<Source*>(data);
  ma_data_source_uninit(&source->base);
  delete source;
}
ma_decoding_backend_vtable kBackend = {Init, nullptr, nullptr, InitMemory,
                                       Uninit};
}  // namespace
ma_decoding_backend_vtable* DecodingBackend() {
  return &kBackend;
}
void ConfigureLoopPoints(ma_decoder& decoder) {
  if (decoder.pBackendVTable != &kBackend)
    return;
  auto& source = *static_cast<Source*>(decoder.pBackend);
  if (!source.midi)
    return;
  // Loop coordinates belong to the outer, converted data source. MIDI emits
  // 48 kHz stereo, but scale explicitly if another caller requests a new rate.
  ma_uint64 start = source.decoder->LoopStart() * decoder.outputSampleRate /
                    source.decoder->Rate();
  ma_uint64 end = source.decoder->LoopEnd() * decoder.outputSampleRate /
                  source.decoder->Rate();
  ma_data_source_set_loop_point_in_pcm_frames(&decoder, start, end);
}
}  // namespace urge::audioformat
