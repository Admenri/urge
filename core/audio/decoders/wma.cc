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

#include "core/audio/decoders/asf.h"
// Decode chain adapted from OxideAV (MIT), Copyright (c) 2026 Karpelès Lab Inc.
// Reference revision and full notice: LICENSES.md.

#include "core/audio/decoders/decoder.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <numbers>

namespace urge::audioformat {
namespace {
constexpr double kPi = std::numbers::pi;
struct Code {
  uint32_t code;
  uint16_t run, level;
  uint8_t bits;
};
#include "core/audio/decoders/wma_tables.inc"
struct Bits {
  Bytes data;
  size_t pos = 0, end = 0;
  bool valid = true;
  uint32_t Read(unsigned n) {
    if (n > 32 || pos > end || n > end - pos) {
      valid = false;
      return 0;
    }
    uint32_t value = 0;
    for (unsigned i = 0; i < n; ++i, ++pos)
      value = (value << 1) | ((data[pos / 8] >> (7 - pos % 8)) & 1);
    return value;
  }
};
class VLC {
 public:
  explicit VLC(std::span<const Code> codes) : codes_(codes), nodes_(1) {
    for (size_t i = 0; i < codes.size(); ++i) {
      int node = 0;
      for (int j = codes[i].bits - 1; j >= 0; --j) {
        unsigned bit = (codes[i].code >> j) & 1;
        if (nodes_[node].next[bit] < 0) {
          int next = static_cast<int>(nodes_.size());
          nodes_[node].next[bit] = next;
          nodes_.push_back({});
        }
        node = nodes_[node].next[bit];
      }
      nodes_[node].symbol = static_cast<int>(i);
    }
  }
  int Read(Bits& bits) const {
    int node = 0;
    for (int depth = 0; depth <= 32 && bits.valid; ++depth) {
      if (nodes_[node].symbol >= 0)
        return nodes_[node].symbol;
      node = nodes_[node].next[bits.Read(1)];
      if (node < 0)
        break;
    }
    bits.valid = false;
    return 0;
  }
  const Code& At(size_t i) const { return codes_[i]; }

 private:
  struct Node {
    int next[2] = {-1, -1};
    int symbol = -1;
  };
  std::span<const Code> codes_;
  std::vector<Node> nodes_;
};
const VLC& Coefficients(int family, bool alt) {
  static const VLC tables[] = {
      VLC(kCOEF_CODES_CLASS1), VLC(kCOEF_CODES_CLASS1_ALT),
      VLC(kCOEF_CODES_CLASS2), VLC(kCOEF_CODES_CLASS2_ALT),
      VLC(kCOEF_CODES_CLASS3), VLC(kCOEF_CODES_CLASS3_ALT)};
  return tables[(family - 1) * 2 + alt];
}
const VLC& Exponents() {
  static const VLC table(kSCALE_CODES);
  return table;
}
const VLC& Gains() {
  static const VLC table(kGAIN_CODES);
  return table;
}
std::vector<int> Bands(uint32_t rate, int m, bool v1) {
  static const int hi128[] = {0, 4, 8, 12, 16, 20, 28, 36, 44, 56, 72, 92, 128};
  static const int hi256[] = {0,  4,  12, 16, 24,  32,  36,  44,
                              52, 64, 76, 88, 112, 140, 180, 256};
  static const int hi512[] = {0,  4,   12,  20,  24,  36,  48,  56,  64,
                              88, 104, 124, 148, 180, 220, 280, 360, 512};
  static const int mid128[] = {0, 4, 8, 16, 20, 24, 36, 52, 76, 96, 124, 128};
  static const int mid256[] = {0,  4,  12,  16,  20,  28,  36,  52,
                               72, 84, 104, 124, 152, 192, 248, 256};
  static const int mid512[] = {0,   8,   12,  20,  28,  40,  56,  76, 100,
                               140, 172, 204, 248, 304, 384, 496, 512};
  static const int lo128[] = {0, 4, 12, 16, 24, 32, 44, 64, 88, 112, 128};
  static const int lo256[] = {0,  4,   12,  20,  24,  36,  48, 64,
                              88, 104, 124, 148, 180, 220, 256};
  std::span<const int> table;
  if (rate >= 44100) {
    if (m == 128)
      table = hi128;
    if (m == 256)
      table = hi256;
    if (m == 512)
      table = hi512;
  } else if (rate >= 32000) {
    if (m == 128)
      table = mid128;
    if (m == 256)
      table = mid256;
    if (m == 512)
      table = mid512;
  } else if (rate >= 22050) {
    if (m == 128)
      table = lo128;
    if (m == 256)
      table = lo256;
  }
  if (!v1 && !table.empty())
    return {table.begin(), table.end()};
  static const int hz[] = {100,  200,   300,   400,  510,  630,  770,
                           920,  1080,  1270,  1480, 1720, 2000, 2320,
                           2700, 3150,  3700,  4400, 5300, 6400, 7700,
                           9500, 12000, 15500, 24500};
  std::vector<int> out{0};
  for (int f : hz) {
    int edge =
        v1 ? std::min(m, int((uint64_t(f) * 2 * m + rate / 2) / rate))
           : std::min(m, int(((uint64_t(f) * 2 * m / rate + 2) >> 2) << 2));
    if (edge > out.back())
      out.push_back(edge);
    if (edge == m)
      break;
  }
  if (out.back() != m)
    out.push_back(m);
  return out;
}
// Radix-2 FFT evaluates the DCT-IV at odd bins of an 8M-point transform.
// No platform SIMD or external FFT dependency is required.
std::vector<double> Inverse(const std::vector<double>& coefficients) {
  size_t m = coefficients.size(), size = m * 8;
  std::vector<std::complex<double>> fft(size);
  for (size_t k = 0; k < m; ++k)
    fft[2 * k + 1] = coefficients[k];
  for (size_t i = 1, j = 0; i < size; ++i) {
    size_t bit = size >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(fft[i], fft[j]);
  }
  for (size_t len = 2; len <= size; len <<= 1) {
    const std::complex<double> root(std::cos(-2 * kPi / len),
                                    std::sin(-2 * kPi / len));
    for (size_t start = 0; start < size; start += len) {
      std::complex<double> w(1, 0);
      for (size_t j = 0; j < len / 2; ++j) {
        auto a = fft[start + j], b = fft[start + j + len / 2] * w;
        fft[start + j] = a + b;
        fft[start + j + len / 2] = a - b;
        w *= root;
      }
    }
  }
  std::vector<double> time(2 * m);
  for (size_t n = 0; n < 2 * m; ++n) {
    size_t t = n + m / 2, k;
    double sign = 1;
    if (t < m)
      k = t;
    else if (t < 2 * m) {
      k = 2 * m - 1 - t;
      sign = -1;
    } else {
      k = t - 2 * m;
      sign = -1;
    }
    time[n] = fft[2 * k + 1].real() * sign * (2.0 / m);
  }
  return time;
}
struct PacketRecord {
  size_t start = 0, own = 0, limit = 0;
  unsigned frames = 0;
};
class WMA final : public Decoder {
 public:
  bool Init(Bytes bytes, std::string& error) {
    WMAStream stream;
    if (!ReadASF(bytes, stream, error))
      return false;
    version_ = stream.version;
    rate_ = stream.rate;
    channels_ = stream.channels;
    flags_ = stream.flags;
    frame_ = rate_ <= 16000                    ? 512
             : rate_ <= 22050                  ? 1024
             : rate_ <= 32000 && version_ == 1 ? 1024
                                               : 2048;
    frame_bits_ = std::bit_width(unsigned(frame_)) - 1;
    bps_ = stream.bytes_per_second * 8.0 / channels_ / rate_;
    double adjusted = channels_ > 1 ? bps_ * 1.6 : bps_;
    family_ = rate_ < 32000 ? 3 : adjusted < 0.72 ? 1 : adjusted < 1.16 ? 2 : 3;
    bool reservoir = flags_ & 2;
    variable_ = reservoir && (flags_ & 4);
    int sizes =
        variable_
            ? std::min(frame_ / 128,
                       (stream.bytes_per_second / channels_ >= 4000 ? 8 : 2)
                           << ((flags_ >> 3) & 3))
            : 1;
    max_index_ = std::bit_width(unsigned(sizes)) - 1;
    size_bits_ = variable_ ? std::bit_width(unsigned(max_index_)) : 0;
    unsigned frame_bytes = std::max(1u, unsigned(std::ceil(frame_ * bps_ / 8)));
    unsigned carry_width = std::bit_width(frame_bytes) + 4;
    if (rate_ >= 44100)
      cutoff_ = adjusted < 0.61 ? 0.4 : 0;
    else if (rate_ >= 22050)
      cutoff_ = adjusted < 1.16 ? (adjusted < 0.72 ? 0.6 : 0.7) : 0;
    else if (rate_ >= 16000)
      cutoff_ = bps_ <= 0.5 ? 0.3 : 0.5;
    else if (rate_ >= 11025)
      cutoff_ = 0.7;
    else if (rate_ >= 8000)
      cutoff_ = bps_ <= 0.75 ? (bps_ <= 0.625 ? 0.5 : 0.65) : 0;
    else
      cutoff_ = bps_ < 0.8 ? 0.75 : 0.6;
    size_t total_bits = 0;
    for (const auto& packet : stream.packets) {
      Bits r{packet, 0, packet.size() * 8};
      unsigned frames = 1, carry = 0;
      if (reservoir) {
        r.Read(4);
        frames = r.Read(4);
        carry = r.Read(carry_width);
      }
      if (!r.valid || carry > r.end - r.pos) {
        error = "invalid WMA bit reservoir";
        return false;
      }
      PacketRecord rec;
      rec.start = total_bits;
      rec.own = total_bits + carry;
      rec.frames = frames;
      while (r.pos < r.end) {
        if (total_bits % 8 == 0)
          bits_.push_back(0);
        bits_.back() |= static_cast<uint8_t>(r.Read(1) << (7 - total_bits % 8));
        ++total_bits;
      }
      records_.push_back(rec);
      length_ += uint64_t(frame_) * frames;
    }
    for (size_t i = 0; i < records_.size(); ++i)
      records_[i].limit =
          i + 1 < records_.size() ? records_[i + 1].own : total_bits;
    if (length_ <= uint64_t(frame_)) {
      error = "empty WMA timeline";
      return false;
    }
    length_ -= frame_;
    if (length_ > uint64_t(rate_) * 60 * 60 * 24 || records_.empty()) {
      error = "WMA duration exceeds limit";
      return false;
    }
    Reset();
    return true;
  }
  size_t Read(float* output, size_t frames) override {
    frames = static_cast<size_t>(std::min<uint64_t>(frames, length_ - cursor_));
    size_t done = 0;
    while (done < frames) {
      if (queue_pos_ == queue_.size()) {
        queue_.clear();
        queue_pos_ = 0;
        if (packet_ < records_.size())
          DecodePacket();
        else if (!flushed_) {
          Emit(position_ - frame_ / 2, frame_);
          flushed_ = true;
        } else
          break;
        if (queue_.empty())
          continue;
      }
      if (priming_) {
        size_t skip =
            std::min(priming_, (queue_.size() - queue_pos_) / channels_);
        queue_pos_ += skip * channels_;
        priming_ -= skip;
        if (queue_pos_ == queue_.size())
          continue;
      }
      size_t n =
          std::min(frames - done, (queue_.size() - queue_pos_) / channels_);
      if (output)
        std::copy_n(queue_.data() + queue_pos_, n * channels_,
                    output + done * channels_);
      queue_pos_ += n * channels_;
      done += n;
      cursor_ += n;
    }
    return done;
  }
  bool Seek(uint64_t frame) override {
    if (frame > Length())
      return false;
    Reset();
    return Read(nullptr, static_cast<size_t>(frame)) == frame;
  }
  uint64_t Cursor() const override { return cursor_; }
  uint64_t Length() const override { return length_; }
  uint64_t DamagedFrames() const override { return damaged_; }
  uint32_t Rate() const override { return rate_; }
  uint32_t Channels() const override { return channels_; }

 private:
  void Reset() {
    damaged_ = 0;
    priming_ = size_t(frame_) * 2;
    packet_ = queue_pos_ = cursor_ = 0;
    queue_.clear();
    position_ = base_ = 0;
    previous_ = next_index_ = -1;
    flushed_ = false;
    noise_ = 0;
    noise_previous_ = 0;
    for (auto& a : accumulator_)
      a.clear();
    last_envelope_ = {};
  }
  void Emit(int64_t from, int count) {
    for (int i = 0; i < count; ++i)
      for (unsigned ch = 0; ch < channels_; ++ch) {
        int64_t p = from + i - base_;
        double value = p >= 0 && uint64_t(p) < accumulator_[ch].size()
                           ? accumulator_[ch][p]
                           : 0;
        queue_.push_back(
            std::isfinite(value) ? float(std::clamp(value, -1.0, 1.0)) : 0);
      }
    int64_t end = from + count;
    if (end > base_) {
      size_t drop = static_cast<size_t>(end - base_);
      for (auto& a : accumulator_)
        a.erase(a.begin(), a.begin() + std::min(drop, a.size()));
      base_ = end;
    }
  }
  double Noise() {
    noise_ = noise_ * 0x19660d + 0x3c6ef35f;
    int32_t s = static_cast<int32_t>(noise_);
    int32_t r = (s >> 2) + (s >> 4);
    int32_t out = static_cast<int32_t>(uint32_t(r) - uint32_t(noise_previous_));
    noise_previous_ = r;
    return out / 536870912.0;
  }
  std::vector<double> Envelope(Bits& r, int m, const std::vector<int>& edges) {
    std::vector<double> out(m);
    if (flags_ & 1) {
      std::vector<int> exponent(edges.size() - 1);
      int prev = version_ == 1 ? int(r.Read(5)) + 10 : 36;
      size_t first = version_ == 1 ? 1 : 0;
      if (first)
        exponent[0] = prev;
      for (size_t i = first; i < exponent.size(); ++i) {
        prev += Exponents().Read(r) - 60;
        if (prev < -1000 || prev > 1000) {
          r.valid = false;
          return {};
        }
        exponent[i] = prev;
      }
      int maximum = *std::max_element(exponent.begin(), exponent.end());
      for (size_t i = 0; i < exponent.size(); ++i)
        std::fill(out.begin() + edges[i], out.begin() + edges[i + 1],
                  std::pow(10.0, std::max(-72, exponent[i] - maximum) / 16.0));
    } else {
      // A(z) = ((1+z^-1)Q(z) + (1-z^-1)P(z))/2, order ten.
      std::array<std::vector<double>, 2> polynomial = {std::vector<double>{1},
                                                       std::vector<double>{1}};
      for (int i = 0; i < 10; ++i) {
        unsigned index = r.Read(i == 0 || i >= 8 ? 3 : 4);
        double coefficient = std::bit_cast<float>(kLSP[i][index]);
        auto& p = polynomial[i & 1];
        std::vector<double> next(p.size() + 2);
        for (size_t k = 0; k < p.size(); ++k) {
          next[k] += p[k];
          next[k + 1] += coefficient * p[k];
          next[k + 2] += p[k];
        }
        p = std::move(next);
      }
      std::array<double, 12> a{};
      for (int i = 0; i < 11; ++i) {
        a[i] += (polynomial[0][i] + polynomial[1][i]) * 0.5;
        a[i + 1] += (polynomial[0][i] - polynomial[1][i]) * 0.5;
      }
      double maximum = 0;
      int grid = flags_ & 32 ? m : frame_;
      for (int i = 0; i < m; ++i) {
        std::complex<double> sum{}, phase(1, 0),
            w(std::cos(kPi * i / grid), std::sin(kPi * i / grid));
        for (double v : a) {
          sum += v * phase;
          phase *= w;
        }
        out[i] = std::pow(std::max(1e-30, std::norm(sum)), -0.25);
        maximum = std::max(maximum, out[i]);
      }
      for (auto& value : out)
        value /= maximum;
    }
    return out;
  }
  bool Block(Bits& r, bool& latch, int remaining) {
    int index = 0, prev_size = previous_, next_size = frame_;
    if (variable_) {
      if (latch) {
        unsigned prev = r.Read(size_bits_);
        index = r.Read(size_bits_);
        if (previous_ < 0)
          prev_size = prev <= unsigned(max_index_) ? frame_ >> prev : frame_;
      } else
        index = next_index_;
      next_index_ = r.Read(size_bits_);
      latch = false;
      if (index < 0 || index > max_index_ || next_index_ > max_index_)
        return false;
      next_size = frame_ >> next_index_;
    }
    const int m = frame_ >> index;
    if (m > remaining)
      return false;
    bool joint = channels_ == 2 && r.Read(1);
    std::array<bool, 2> coded{};
    for (unsigned ch = 0; ch < channels_; ++ch)
      coded[ch] = r.Read(1);
    std::array<std::vector<double>, 2> spectrum{std::vector<double>(m),
                                                std::vector<double>(m)};
    if (coded[0] || coded[1]) {
      unsigned gain = 1, value;
      do {
        value = r.Read(7);
        gain += value;
      } while (value == 127 && r.valid && gain < 256);
      if (gain >= 256 || !r.valid)
        return false;
      int begin = version_ == 1 ? 3 * m / frame_ : 0, end = m - 9 * m / 100;
      auto edges = Bands(rate_, m, version_ == 1);
      std::vector<std::pair<int, int>> high;
      if (cutoff_) {
        int cut = int(m * cutoff_ + 0.5);
        for (size_t i = 0; i + 1 < edges.size() && edges[i] < end; ++i)
          if (edges[i + 1] > cut)
            high.emplace_back(std::max(edges[i], cut),
                              std::min(edges[i + 1], end));
      }
      std::array<std::vector<bool>, 2> noise_flags;
      std::array<std::vector<int>, 2> noise_gains;
      for (unsigned ch = 0; ch < channels_; ++ch)
        for (size_t i = 0; coded[ch] && i < high.size(); ++i)
          noise_flags[ch].push_back(r.Read(1));
      for (unsigned ch = 0; ch < channels_; ++ch) {
        int previous = 0;
        bool first = true;
        for (bool flagged : noise_flags[ch])
          if (flagged) {
            previous =
                first ? int(r.Read(7)) - 19 : previous + Gains().Read(r) - 18;
            if (previous < -256 || previous > 256)
              return false;
            first = false;
            noise_gains[ch].push_back(previous);
          }
      }
      bool new_envelope = true;
      if (m < frame_ && variable_ && (cutoff_ || channels_ == 2))
        new_envelope = r.Read(1);
      std::array<std::vector<double>, 2> env;
      for (unsigned ch = 0; ch < channels_; ++ch) {
        if (!coded[ch])
          continue;
        // Reuse the most recent channel envelope, resampling when the block
        // size changes. Keeping a separate old envelope per size causes
        // audible transient errors when short/long blocks alternate.
        if (new_envelope) {
          env[ch] = Envelope(r, m, edges);
        } else {
          env[ch].resize(m, 1);
          if (!last_envelope_[ch].empty())
            for (int i = 0; i < m; ++i)
              env[ch][i] =
                  last_envelope_[ch][size_t(i) * last_envelope_[ch].size() / m];
        }
        if (env[ch].size() != size_t(m))
          return false;
        last_envelope_[ch] = env[ch];
      }
      const unsigned level_bits = gain <= 14   ? 13
                                  : gain <= 31 ? 12
                                  : gain <= 39 ? 11
                                  : gain <= 44 ? 10
                                               : 9;
      for (unsigned ch = 0; ch < channels_; ++ch) {
        if (!coded[ch])
          continue;
        std::vector<int> flags(m, -1);
        size_t band_index = 0;
        for (size_t i = 0; i < noise_flags[ch].size(); ++i)
          if (noise_flags[ch][i]) {
            for (int k = high[i].first; k < high[i].second; ++k)
              flags[k] = static_cast<int>(band_index);
            ++band_index;
          }
        int count = 0;
        for (int k = begin; k < end; ++k)
          if (flags[k] < 0)
            ++count;
        std::vector<int> quantized(count);
        const VLC& vlc = Coefficients(family_, joint && ch == 1);
        int at = 0;
        while (at < count && r.valid) {
          int symbol = vlc.Read(r);
          if (symbol == 1)
            break;
          unsigned level, run;
          if (!symbol) {
            level = r.Read(level_bits);
            run = r.Read(frame_bits_);
          } else {
            level = vlc.At(symbol).level;
            run = vlc.At(symbol).run;
          }
          bool positive = r.Read(1);
          if (run >= unsigned(count - at))
            return false;
          at += run;
          quantized[at++] = positive ? int(level) : -int(level);
        }
        if (version_ == 1 && channels_ == 2)
          r.Read(unsigned((8 - r.pos % 8) % 8));
        if (!r.valid)
          return false;
        std::vector<double> noise_scale(noise_gains[ch].size());
        double last_energy = 0;
        size_t last_count = 0;
        for (int k = 0; k < m; ++k)
          if (flags[k] >= 0 && size_t(flags[k]) + 1 == noise_scale.size()) {
            last_energy += env[ch][k] * env[ch][k];
            ++last_count;
          }
        last_energy /= std::max<size_t>(1, last_count);
        for (size_t b = 0; b < noise_scale.size(); ++b) {
          double energy = 0;
          size_t n = 0;
          for (int k = 0; k < m; ++k)
            if (flags[k] == int(b)) {
              energy += env[ch][k] * env[ch][k];
              ++n;
            }
          double ratio =
              last_energy > 0
                  ? std::sqrt(energy / std::max<size_t>(1, n) / last_energy)
                  : 1;
          noise_scale[b] =
              ratio * std::pow(10.0, std::max(0, noise_gains[ch][b]) / 20.0);
        }
        double total = std::pow(10.0, gain / 20.0);
        double scale = -1.0 / 32768;
        if (version_ == 1)
          scale *= std::sqrt(m / 2.0);
        at = 0;
        for (int k = 0; k < m; ++k) {
          double noise = cutoff_ ? Noise() : 0;
          int q = k >= begin && k < end && flags[k] < 0 ? quantized[at++] : 0;
          double v;
          if (flags[k] >= 0)
            v = noise * env[ch][k] * noise_scale[flags[k]];
          else if (k < begin || k >= end)
            v = noise * 0.02 * env[ch][std::clamp(k, begin, end - 1)] * total;
          else
            v = (q + noise * (flags_ & 1 ? 0.02 : 0.04)) * env[ch][k] * total;
          spectrum[ch][k] = v * scale;
        }
      }
    }
    if (!r.valid)
      return false;
    if (joint)
      for (int i = 0; i < m; ++i) {
        double mid = spectrum[0][i], side = spectrum[1][i];
        spectrum[0][i] = mid + side;
        spectrum[1][i] = mid - side;
      }
    if (prev_size < 0)
      prev_size = m;
    for (unsigned ch = 0; ch < channels_; ++ch) {
      auto time = Inverse(spectrum[ch]);
      double rise = std::min(m, prev_size), fall = std::min(m, next_size);
      int64_t start = position_ - m / 2;
      size_t need = static_cast<size_t>(position_ + 3 * m / 2 - base_);
      accumulator_[ch].resize(std::max(need, accumulator_[ch].size()), 0);
      for (int i = 0; i < 2 * m; ++i) {
        double x = i + 0.5, w;
        if (x < m / 2.0 - rise / 2)
          w = 0;
        else if (x < m / 2.0 + rise / 2)
          w = std::sin(kPi / 2 * (x - (m / 2.0 - rise / 2)) / rise);
        else if (x < 3 * m / 2.0 - fall / 2)
          w = 1;
        else if (x < 3 * m / 2.0 + fall / 2)
          w = std::sin(kPi / 2 * (1 - (x - (3 * m / 2.0 - fall / 2)) / fall));
        else
          w = 0;
        if (start + i >= base_)
          accumulator_[ch][size_t(start + i - base_)] += time[i] * w;
      }
    }
    previous_ = m;
    Emit(position_ - frame_ / 2, m);
    position_ += m;
    remaining_block_ = m;
    return true;
  }
  void DecodePacket() {
    const auto& record = records_[packet_++];
    Bits r{bits_, record.own, record.limit};
    bool latch = true;
    bool failed = false;
    for (unsigned frame = 0; frame < record.frames; ++frame) {
      int remaining = frame_;
      while (!failed && remaining > 0) {
        if (!Block(r, latch, remaining)) {
          failed = true;
          break;
        }
        remaining -= remaining_block_;
      }
      if (failed) {
        ++damaged_;
        // Damaged packets keep their declared timing and resynchronise at the
        // next ASF/WMA packet boundary; never send invalid samples to the
        // mixer.
        for (auto& a : accumulator_)
          a.clear();
        base_ = position_;
        queue_.insert(queue_.end(), size_t(remaining) * channels_, 0);
        position_ += remaining;
        previous_ = next_index_ = -1;
        last_envelope_ = {};
      }
    }
  }
  uint16_t version_ = 2, flags_ = 0;
  uint32_t rate_ = 0, channels_ = 0;
  int frame_ = 2048, frame_bits_ = 11, family_ = 3, size_bits_ = 0,
      max_index_ = 0;
  bool variable_ = false, flushed_ = false;
  double bps_ = 0, cutoff_ = 0;
  std::vector<uint8_t> bits_;
  std::vector<PacketRecord> records_;
  size_t packet_ = 0, queue_pos_ = 0, priming_ = 0;
  uint64_t cursor_ = 0, length_ = 0, damaged_ = 0;
  int previous_ = -1, next_index_ = -1, remaining_block_ = 0;
  int64_t position_ = 0, base_ = 0;
  uint32_t noise_ = 0;
  int32_t noise_previous_ = 0;
  std::array<std::vector<double>, 2> accumulator_, last_envelope_;
  std::vector<float> queue_;
};
}  // namespace
std::unique_ptr<Decoder> OpenWMA(Bytes bytes, std::string& error) {
  auto decoder = std::make_unique<WMA>();
  if (!decoder->Init(bytes, error))
    return {};
  return decoder;
}
}  // namespace urge::audioformat
