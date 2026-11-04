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

#include "core/audio/decoders/reader.h"
#include "core/audio/decoders/soundfont.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace urge::audioformat {
namespace {
constexpr uint32_t kRate = 48000;
constexpr size_t kMaxEvents = 1000000;
constexpr uint64_t kMaxFrames = uint64_t(kRate) * 60 * 60 * 24;
struct Event {
  uint64_t tick = 0, frame = 0;
  uint32_t order = 0, tempo = 0;
  uint8_t status = 0, a = 0, b = 0;
};
class MIDIReader {
 public:
  explicit MIDIReader(Bytes bytes) : bytes_(bytes) {}
  uint8_t Byte() {
    if (pos_ >= bytes_.size()) {
      valid_ = false;
      return 0;
    }
    return bytes_[pos_++];
  }
  uint32_t VLQ() {
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
      uint8_t byte = Byte();
      result = (result << 7) | (byte & 127);
      if (!(byte & 128))
        return result;
    }
    valid_ = false;
    return 0;
  }
  bool Skip(size_t n) {
    if (!Range(bytes_, pos_, n)) {
      valid_ = false;
      return false;
    }
    pos_ += n;
    return true;
  }
  bool More() const { return valid_ && pos_ < bytes_.size(); }
  bool Valid() const { return valid_; }

 private:
  Bytes bytes_;
  size_t pos_ = 0;
  bool valid_ = true;
};
Bytes Unwrap(Bytes bytes) {
  Bytes body;
  if (!RIFF(bytes, "RMID", body))
    return bytes;
  Chunks chunks(body);
  Chunk chunk;
  while (chunks.Next(chunk))
    if (chunk.Is("data"))
      return chunk.data;
  return {};
}
bool ParseMIDI(Bytes bytes,
               std::vector<Event>& events,
               uint64_t& end,
               uint64_t& loop) {
  if (bytes.size() > 64 * 1024 * 1024)
    return false;
  bytes = Unwrap(bytes);
  if (!Tag(bytes, 0, "MThd") || !Range(bytes, 8, BE32(bytes, 4)) ||
      BE32(bytes, 4) < 6 || BE16(bytes, 8) > 1)
    return false;
  const uint16_t format = BE16(bytes, 8), tracks = BE16(bytes, 10),
                 division = BE16(bytes, 12);
  if (!tracks || tracks > 1024 || (!format && tracks != 1) || !division)
    return false;
  size_t pos = 8 + BE32(bytes, 4);
  uint64_t last_tick = 0;
  uint32_t order = 0;
  for (unsigned t = 0; t < tracks; ++t) {
    if (!Tag(bytes, pos, "MTrk") ||
        !Range(bytes, pos + 8, BE32(bytes, pos + 4)))
      return false;
    MIDIReader r(bytes.subspan(pos + 8, BE32(bytes, pos + 4)));
    pos += 8 + BE32(bytes, pos + 4);
    uint8_t running = 0;
    uint64_t tick = 0;
    bool eot = false;
    while (r.More()) {
      uint32_t delta = r.VLQ();
      if (tick > (uint64_t(1) << 48) - delta)
        return false;
      tick += delta;
      Event event;
      event.tick = tick;
      event.order = order++;
      uint8_t status = r.Byte(), first = 0;
      bool has_first = !(status & 128);
      if (has_first) {
        first = status;
        status = running;
      }
      if (status < 128)
        return false;
      if (status == 0xff) {
        running = 0;
        uint8_t kind = r.Byte();
        uint32_t size = r.VLQ();
        if (kind == 0x51) {
          if (size != 3)
            return false;
          event.tempo = uint32_t(r.Byte()) << 16;
          event.tempo |= uint32_t(r.Byte()) << 8;
          event.tempo |= r.Byte();
          if (!event.tempo)
            return false;
          events.push_back(event);
        } else {
          if (kind == 0x2f && size != 0)
            return false;
          if (!r.Skip(size))
            return false;
        }
        if (kind == 0x2f) {
          eot = true;
          break;
        }
      } else if (status == 0xf0 || status == 0xf7) {
        running = 0;
        uint32_t size = r.VLQ();
        if (!r.Skip(size))
          return false;
      } else {
        if (status >= 0xf0)
          return false;
        running = status;
        event.status = status;
        event.a = has_first ? first : r.Byte();
        if ((status & 0xf0) != 0xc0 && (status & 0xf0) != 0xd0)
          event.b = r.Byte();
        // Legacy exporters sometimes leave the spare high bit set in channel
        // data (notably pitch bend). Consume the status-defined width, then
        // mask parameters, as TinyMidiLoader does; do not guess running status.
        event.a &= 127;
        event.b &= 127;
        events.push_back(event);
      }
      if (!r.Valid() || events.size() > kMaxEvents)
        return false;
    }
    if (!r.Valid() || !eot)
      return false;
    last_tick = std::max(last_tick, tick);
  }
  std::stable_sort(
      events.begin(), events.end(),
      [](const Event& a, const Event& b) { return a.tick < b.tick; });
  double smpte = 0;
  if (division & 0x8000) {
    const int fps = -static_cast<int8_t>(division >> 8);
    if ((fps != 24 && fps != 25 && fps != 29 && fps != 30) || !(division & 255))
      return false;
    smpte = (fps == 29 ? 30000.0 / 1001 : fps) * (division & 255);
  }
  uint64_t previous = 0;
  long double frame = 0;
  uint32_t tempo = 500000;
  bool marked = false;
  auto advance = [&](uint64_t tick) {
    frame += (tick - previous) * static_cast<long double>(kRate) *
             (smpte ? 1.0L / smpte
                    : static_cast<long double>(tempo) / division / 1000000);
    previous = tick;
    return frame <= kMaxFrames;
  };
  for (auto& event : events) {
    if (!advance(event.tick))
      return false;
    event.frame = static_cast<uint64_t>(std::round(frame));
    if (event.tempo)
      tempo = event.tempo;
    if (!marked && (event.status & 0xf0) == 0xb0 && event.a == 111) {
      loop = event.frame;
      marked = true;
    }
  }
  if (!advance(last_tick))
    return false;
  end = std::max<uint64_t>(1, static_cast<uint64_t>(std::round(frame)));
  if (loop >= end)
    loop = 0;
  return true;
}
struct Channel {
  int program = 0, bank_msb = 0, bank_lsb = 0;
  int volume = 100, expression = 127, pan = 64, pitch = 8192;
  int rpn_msb = 127, rpn_lsb = 127, bend_range = 2, bend_cents = 0;
  bool sustain = false;
};
struct Voice {
  const Region* region = nullptr;
  int channel = 0, key = 0, velocity = 0;
  bool down = true, released = false;
  uint64_t serial = 0;
  double position = 0, age = 0, release_age = 0;
  float release_gain = 0;
};
class Synth {
 public:
  explicit Synth(std::shared_ptr<const SoundBank> bank)
      : bank_(std::move(bank)) {
    Reset();
  }
  void Reset() {
    channels_ = {};
    voices_ = {};
    serial_ = 0;
  }
  void EventAt(const Event& event) {
    int index = event.status & 15;
    Channel& c = channels_[index];
    int a = event.a, b = event.b;
    switch (event.status & 0xf0) {
      case 0x80:
        Off(index, a);
        break;
      case 0x90:
        if (b)
          On(index, a, b);
        else
          Off(index, a);
        break;
      case 0xc0:
        c.program = a;
        break;
      case 0xe0:
        c.pitch = a | (b << 7);
        break;
      case 0xb0:
        switch (a) {
          case 0:
            c.bank_msb = b;
            break;
          case 32:
            c.bank_lsb = b;
            break;
          case 7:
            c.volume = b;
            break;
          case 10:
            c.pan = b;
            break;
          case 11:
            c.expression = b;
            break;
          case 64:
            c.sustain = b >= 64;
            if (!c.sustain)
              for (auto& v : voices_)
                if (v.region && v.channel == index && !v.down)
                  Release(v);
            break;
          case 100:
            c.rpn_lsb = b;
            break;
          case 101:
            c.rpn_msb = b;
            break;
          case 6:
            if (c.rpn_msb == 0 && c.rpn_lsb == 0)
              c.bend_range = std::min(b, 24);
            break;
          case 38:
            if (c.rpn_msb == 0 && c.rpn_lsb == 0)
              c.bend_cents = std::min(b, 99);
            break;
          case 120:
            for (auto& v : voices_)
              if (v.channel == index)
                v.region = nullptr;
            break;
          case 121:
            c.expression = 127;
            c.pitch = 8192;
            c.sustain = false;
            c.rpn_msb = c.rpn_lsb = 127;
            for (auto& v : voices_)
              if (v.region && v.channel == index && !v.down)
                Release(v);
            break;
          case 123:
            for (auto& v : voices_)
              if (v.region && v.channel == index) {
                v.down = false;
                if (!c.sustain)
                  Release(v);
              }
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }
  void End() {
    for (auto& v : voices_)
      if (v.region)
        Release(v);
  }
  void Render(float* output, size_t frames) {
    if (output)
      std::fill_n(output, frames * 2, 0.0f);
    for (auto& voice : voices_) {
      if (!voice.region)
        continue;
      const Region& r = *voice.region;
      const Channel& c = channels_[voice.channel];
      double bend =
          (c.pitch - 8192) / 8192.0 * (c.bend_range + c.bend_cents / 100.0);
      double step =
          r.rate / double(kRate) *
          std::exp2(((voice.key - r.root) * r.scale + r.tune + bend * 100) /
                    1200.0);
      float volume = (c.volume / 127.0f) * (c.expression / 127.0f);
      volume *= volume * (voice.velocity / 127.0f) * r.gain * 0.35f;
      const float pan = std::clamp(r.pan + (c.pan - 64) / 64.0f, -1.0f, 1.0f);
      const float left = std::sqrt((1 - pan) * 0.5f) * volume;
      const float right = std::sqrt((1 + pan) * 0.5f) * volume;
      if (!output) {
        voice.position += step * frames;
        Advance(voice, frames / double(kRate));
        Position(voice);
        continue;
      }
      for (size_t i = 0; i < frames && voice.region; ++i) {
        if (!Position(voice))
          break;
        size_t at = static_cast<size_t>(voice.position);
        size_t next = std::min<size_t>(at + 1, r.end - 1);
        if (Loops(voice) && next == r.loop_end)
          next = r.loop_start;
        float sample =
            bank_->samples[at] + (bank_->samples[next] - bank_->samples[at]) *
                                     float(voice.position - at);
        float gain = Envelope(voice);
        output[2 * i] += sample * gain * left;
        output[2 * i + 1] += sample * gain * right;
        voice.position += step;
        Advance(voice, 1.0 / kRate);
      }
    }
  }

 private:
  float Envelope(const Voice& v) const {
    const Region& r = *v.region;
    if (v.released)
      return v.release_gain *
             float(std::max(0.0,
                            1 - v.release_age / std::max(0.00001f, r.release)));
    if (v.age < r.attack)
      return float(v.age / std::max(0.00001f, r.attack));
    if (v.age < r.attack + r.hold)
      return 1;
    if (v.age < r.attack + r.hold + r.decay)
      return 1 - (1 - r.sustain) * float((v.age - r.attack - r.hold) /
                                         std::max(0.00001f, r.decay));
    return r.sustain;
  }
  void Release(Voice& v) {
    if (v.released)
      return;
    v.release_gain = Envelope(v);
    v.released = true;
    v.release_age = 0;
  }
  void Advance(Voice& v, double seconds) {
    v.age += seconds;
    if (v.released) {
      v.release_age += seconds;
      if (v.release_age >= v.region->release)
        v.region = nullptr;
    }
  }
  bool Loops(const Voice& v) const {
    return v.region &&
           (v.region->loop == 1 || (v.region->loop == 3 && !v.released));
  }
  bool Position(Voice& v) {
    if (!v.region)
      return false;
    const Region& r = *v.region;
    if (Loops(v) && v.position >= r.loop_end)
      v.position = r.loop_start + std::fmod(v.position - r.loop_start,
                                            r.loop_end - r.loop_start);
    if (v.position >= r.end) {
      v.region = nullptr;
      return false;
    }
    return true;
  }
  void Off(int ch, int key) {
    for (auto& v : voices_)
      if (v.region && v.channel == ch && v.key == key && v.down) {
        v.down = false;
        if (!channels_[ch].sustain)
          Release(v);
      }
  }
  void On(int ch, int key, int velocity) {
    const Channel& c = channels_[ch];
    bool drums = ch == 9;
    int bank = drums ? 128 : c.bank_msb * 128 + c.bank_lsb;
    int program = c.program;
    auto matches = [&](const Region& r, bool fallback) {
      return r.drums == drums &&
             r.program == (fallback && drums ? 0 : program) &&
             (fallback ? (drums || r.bank == 0) : r.bank == bank) &&
             key >= r.key_lo && key <= r.key_hi && velocity >= r.vel_lo &&
             velocity <= r.vel_hi;
    };
    bool exact =
        std::any_of(bank_->regions.begin(), bank_->regions.end(),
                    [&](const Region& r) { return matches(r, false); });
    // Choke exclusive classes before layering this note, not between its zones.
    for (const Region& r : bank_->regions) {
      if (!matches(r, !exact) || !r.exclusive)
        continue;
      for (auto& v : voices_)
        if (v.region && v.channel == ch && v.region->exclusive == r.exclusive)
          v.region = nullptr;
    }
    for (const Region& r : bank_->regions) {
      if (!matches(r, !exact))
        continue;
      Voice* v = nullptr;
      for (auto& candidate : voices_)
        if (!candidate.region) {
          v = &candidate;
          break;
        }
      if (!v)
        v = &*std::min_element(
            voices_.begin(), voices_.end(),
            [](const Voice& a, const Voice& b) { return a.serial < b.serial; });
      *v = {};
      v->region = &r;
      v->channel = ch;
      v->key = key;
      v->velocity = velocity;
      v->position = r.start;
      v->serial = ++serial_;
    }
  }
  std::shared_ptr<const SoundBank> bank_;
  std::array<Channel, 16> channels_{};
  std::array<Voice, 128> voices_{};
  uint64_t serial_ = 0;
};
class MIDI final : public Decoder {
 public:
  explicit MIDI(std::shared_ptr<const SoundBank> bank)
      : synth_(std::move(bank)) {}
  bool Init(Bytes bytes) { return ParseMIDI(bytes, events_, end_, loop_); }
  size_t Read(float* out, size_t frames) override {
    frames =
        static_cast<size_t>(std::min<uint64_t>(frames, Length() - cursor_));
    size_t done = 0;
    while (done < frames) {
      while (event_ < events_.size() && events_[event_].frame <= cursor_)
        synth_.EventAt(events_[event_++]);
      if (cursor_ >= end_ && !ended_) {
        synth_.End();
        ended_ = true;
      }
      uint64_t next = Length();
      if (event_ < events_.size())
        next = std::min(next, events_[event_].frame);
      if (!ended_)
        next = std::min(next, end_);
      size_t amount = static_cast<size_t>(
          std::min<uint64_t>(frames - done, next - cursor_));
      if (!amount)
        break;
      synth_.Render(out ? out + done * 2 : nullptr, amount);
      done += amount;
      cursor_ += amount;
    }
    return done;
  }
  bool Seek(uint64_t frame) override {
    if (frame > Length())
      return false;
    synth_.Reset();
    cursor_ = 0;
    event_ = 0;
    ended_ = false;
    return Read(nullptr, static_cast<size_t>(frame)) == frame;
  }
  uint64_t Cursor() const override { return cursor_; }
  uint64_t Length() const override { return end_ + kRate * 2; }
  uint64_t LoopStart() const override { return loop_; }
  uint64_t LoopEnd() const override { return end_; }
  uint32_t Rate() const override { return kRate; }
  uint32_t Channels() const override { return 2; }

 private:
  Synth synth_;
  std::vector<Event> events_;
  size_t event_ = 0;
  uint64_t cursor_ = 0, end_ = 0, loop_ = 0;
  bool ended_ = false;
};
}  // namespace
bool IsMIDI(Bytes bytes) {
  return Tag(bytes, 0, "MThd") ||
         (Tag(bytes, 0, "RIFF") && Tag(bytes, 8, "RMID"));
}
std::unique_ptr<Decoder> OpenMIDI(Bytes bytes,
                                  std::shared_ptr<const SoundBank> bank,
                                  std::string& error) {
  if (!bank || bank->regions.empty()) {
    error = "MIDI disabled: no usable soundfont";
    return {};
  }
  auto midi = std::make_unique<MIDI>(std::move(bank));
  if (!midi->Init(bytes)) {
    error = "invalid or unsupported Standard MIDI file";
    return {};
  }
  return midi;
}
}  // namespace urge::audioformat
