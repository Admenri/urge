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

#include "core/audio/decoders/soundfont.h"
#include "core/audio/decoders/reader.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace urge::audioformat {
namespace {
constexpr size_t kMaxSamples = 64 * 1024 * 1024;
constexpr size_t kMaxRegions = 65536;
using List = std::vector<Chunk>;
bool Children(Bytes data, List& chunks) {
  Chunks reader(data);
  Chunk chunk;
  while (reader.Next(chunk)) {
    if (chunks.size() >= 65536)
      return false;
    chunks.push_back(chunk);
  }
  return reader.valid();
}
Bytes Find(const List& chunks, const char* tag, bool list = false) {
  for (const auto& chunk : chunks)
    if (list ? chunk.List(tag) : chunk.Is(tag))
      return list ? chunk.data.subspan(4) : chunk.data;
  return {};
}
float Timecents(double value) {
  if (value <= -32768)
    return 0;
  return static_cast<float>(std::clamp(std::exp2(value / 1200), 0.0, 100.0));
}
float Gain(double centibels) {
  return static_cast<float>(
      std::pow(10, std::clamp(centibels, -1440.0, 240.0) / 200));
}
bool ValidRegion(Region& r, size_t size) {
  if (r.key_lo > r.key_hi || r.vel_lo > r.vel_hi || r.key_hi > 127 ||
      r.vel_hi > 127 || r.start >= r.end || r.end > size || r.rate < 400 ||
      r.rate > 384000 || r.root < 0 || r.root > 127 || !std::isfinite(r.tune) ||
      std::abs(r.tune) > 24000 || !std::isfinite(r.scale) || r.scale < 0 ||
      r.scale > 1200)
    return false;
  if (r.loop && (r.loop_start < r.start || r.loop_end > r.end ||
                 r.loop_start + 1 >= r.loop_end))
    return false;
  return true;
}
struct Wave {
  Region region;
  bool has_wsmp = false;
};
bool WaveSample(Bytes b, Region& r) {
  if (b.size() < 20 || LE32(b, 0) < 20 || LE32(b, 0) > b.size())
    return false;
  r.root = LE16(b, 4);
  r.tune = static_cast<int16_t>(LE16(b, 6));
  r.gain *= Gain(static_cast<int32_t>(LE32(b, 8)) / 65536.0);
  const uint32_t count = LE32(b, 16);
  if (count > 1)
    return false;
  if (count) {
    size_t p = LE32(b, 0);
    if (!Range(b, p, 16) || LE32(b, p) < 16 || !Range(b, p, LE32(b, p)) ||
        LE32(b, p + 4) != 0)
      return false;
    uint64_t begin = uint64_t(r.start) + LE32(b, p + 8);
    uint64_t end = begin + LE32(b, p + 12);
    if (end > r.end || begin + 1 >= end)
      return false;
    r.loop_start = static_cast<uint32_t>(begin);
    r.loop_end = static_cast<uint32_t>(end);
    r.loop = 1;
  } else {
    r.loop = 0;
  }
  return true;
}
bool Articulations(const List& chunks, Region& region) {
  for (const auto& list : chunks) {
    if (!list.List("lart") && !list.List("lar2"))
      continue;
    List arts;
    if (!Children(list.data.subspan(4), arts))
      return false;
    for (const auto& art : arts) {
      if (!art.Is("art1") && !art.Is("art2"))
        continue;
      Bytes b = art.data;
      if (b.size() < 8 || LE32(b, 0) < 8 || LE32(b, 0) > b.size() ||
          LE32(b, 4) > (b.size() - LE32(b, 0)) / 12)
        return false;
      for (size_t i = 0; i < LE32(b, 4); ++i) {
        const size_t p = LE32(b, 0) + 12 * i;
        // Minimal articulation subset: constant gain/pan and EG1 ADSR.
        if (LE16(b, p) || LE16(b, p + 2) || LE16(b, p + 6))
          continue;
        const double scale = static_cast<int32_t>(LE32(b, p + 8)) / 65536.0;
        switch (LE16(b, p + 4)) {
          case 1:
            region.gain *= Gain(scale);
            break;
          case 4:
            region.pan = std::clamp(float(scale / 500), -1.0f, 1.0f);
            break;
          case 0x206:
            region.attack = Timecents(scale);
            break;
          case 0x207:
            region.decay = Timecents(scale);
            break;
          case 0x209:
            region.release = Timecents(scale);
            break;
          case 0x20a:
            region.sustain = std::clamp(float(scale / 1000), 0.0f, 1.0f);
            break;
          default:
            break;
        }
      }
    }
  }
  return true;
}
bool DLS(Bytes data, SoundBank& bank) {
  List root, waves, instruments;
  if (!Children(data, root) || !Children(Find(root, "wvpl", true), waves) ||
      !Children(Find(root, "lins", true), instruments))
    return false;
  Bytes pool = Find(root, "ptbl");
  if (pool.size() < 8 || LE32(pool, 0) < 8 || LE32(pool, 0) > pool.size() ||
      LE32(pool, 4) > (pool.size() - LE32(pool, 0)) / 4)
    return false;
  std::map<uint32_t, Wave> offsets;
  uint32_t offset = 0;
  for (const auto& chunk : waves) {
    if (chunk.List("wave")) {
      List fields;
      if (!Children(chunk.data.subspan(4), fields))
        return false;
      Bytes fmt = Find(fields, "fmt "), pcm = Find(fields, "data");
      if (fmt.size() < 16 || LE16(fmt, 0) != 1 ||
          (LE16(fmt, 2) != 1 && LE16(fmt, 2) != 2) ||
          (LE16(fmt, 14) != 8 && LE16(fmt, 14) != 16 && LE16(fmt, 14) != 24 &&
           LE16(fmt, 14) != 32))
        return false;
      const size_t channels = LE16(fmt, 2), width = LE16(fmt, 14) / 8;
      const size_t stride = channels * width;
      if (LE16(fmt, 12) != stride || pcm.empty() || pcm.size() % stride)
        return false;
      const size_t frames = pcm.size() / stride;
      if (frames > kMaxSamples - bank.samples.size())
        return false;
      Wave wave;
      wave.region.start = static_cast<uint32_t>(bank.samples.size());
      wave.region.end = static_cast<uint32_t>(bank.samples.size() + frames);
      wave.region.rate = LE32(fmt, 4);
      for (size_t frame = 0; frame < frames; ++frame) {
        double value = 0;
        for (size_t ch = 0; ch < channels; ++ch) {
          size_t p = frame * stride + ch * width;
          if (width == 1)
            value += (int(pcm[p]) - 128) / 128.0;
          if (width == 2)
            value += static_cast<int16_t>(LE16(pcm, p)) / 32768.0;
          if (width == 3) {
            int32_t n = pcm[p] | (uint32_t(pcm[p + 1]) << 8) |
                        (uint32_t(pcm[p + 2]) << 16);
            if (n & 0x800000)
              n -= 0x1000000;
            value += n / 8388608.0;
          }
          if (width == 4)
            value += static_cast<int32_t>(LE32(pcm, p)) / 2147483648.0;
        }
        bank.samples.push_back(static_cast<float>(value / channels));
      }
      Bytes wsmp = Find(fields, "wsmp");
      wave.has_wsmp = !wsmp.empty();
      if (wave.has_wsmp && !WaveSample(wsmp, wave.region))
        return false;
      if (!ValidRegion(wave.region, bank.samples.size()))
        return false;
      offsets.emplace(offset, wave);
    }
    offset +=
        static_cast<uint32_t>(chunk.bytes.size() + (chunk.data.size() & 1));
  }
  for (const auto& instrument : instruments) {
    if (!instrument.List("ins "))
      continue;
    List fields, regions;
    if (!Children(instrument.data.subspan(4), fields) ||
        !Children(Find(fields, "lrgn", true), regions))
      return false;
    Bytes header = Find(fields, "insh");
    if (header.size() < 12 || LE32(header, 8) > 127)
      return false;
    const uint32_t dls_bank = LE32(header, 4);
    for (const auto& zone : regions) {
      if (!zone.List("rgn ") && !zone.List("rgn2"))
        continue;
      List props;
      if (!Children(zone.data.subspan(4), props))
        return false;
      Bytes rgnh = Find(props, "rgnh"), wlnk = Find(props, "wlnk");
      if (rgnh.size() < 12 || wlnk.size() < 12 ||
          LE32(wlnk, 8) >= LE32(pool, 4))
        return false;
      uint32_t index = LE32(pool, LE32(pool, 0) + LE32(wlnk, 8) * 4);
      auto wave = offsets.find(index);
      if (wave == offsets.end())
        return false;
      Region r = wave->second.region;
      r.bank = ((dls_bank >> 8) & 127) * 128 + (dls_bank & 127);
      r.drums = (dls_bank & 0x80000000u) != 0;
      r.program = static_cast<uint8_t>(LE32(header, 8));
      if (LE16(rgnh, 0) > 127 || LE16(rgnh, 2) > 127 || LE16(rgnh, 4) > 127 ||
          LE16(rgnh, 6) > 127)
        return false;
      r.key_lo = LE16(rgnh, 0);
      r.key_hi = LE16(rgnh, 2);
      r.vel_lo = LE16(rgnh, 4);
      r.vel_hi = LE16(rgnh, 6);
      r.exclusive = LE16(rgnh, 10);
      Bytes wsmp = Find(props, "wsmp");
      if (!wsmp.empty()) {
        // A region's wavesample replaces the wave's tuning/loop/gain.
        r.gain = 1;
        r.tune = 0;
        if (!WaveSample(wsmp, r))
          return false;
      }
      if (!Articulations(fields, r) || !Articulations(props, r) ||
          !ValidRegion(r, bank.samples.size()) ||
          bank.regions.size() == kMaxRegions)
        return false;
      bank.regions.push_back(r);
    }
  }
  return !bank.regions.empty();
}

struct Generators {
  std::array<int, 61> value{};
  std::array<bool, 61> set{};
  bool Read(Bytes data, size_t first, size_t last) {
    if (first > last || last > data.size() / 4)
      return false;
    for (size_t i = first; i < last; ++i) {
      uint16_t op = LE16(data, 4 * i);
      if (op >= 61)
        continue;
      const uint16_t n = LE16(data, 4 * i + 2);
      value[op] = (op == 41 || op == 43 || op == 44 || op == 53 || op == 54 ||
                   op == 57 || op == 58)
                      ? n
                      : static_cast<int16_t>(n);
      set[op] = true;
    }
    return true;
  }
  int Get(int op, int fallback = 0) const {
    return set[op] ? value[op] : fallback;
  }
};
Generators Overlay(Generators global, const Generators& zone) {
  for (size_t i = 0; i < 61; ++i)
    if (zone.set[i]) {
      global.value[i] = zone.value[i];
      global.set[i] = true;
    }
  return global;
}
bool Zones(Bytes bags,
           Bytes gens,
           size_t first,
           size_t last,
           std::vector<Generators>& out,
           size_t& budget) {
  if (first > last || !Range(bags, last * 4, 4) || last - first > budget)
    return false;
  budget -= last - first;
  for (size_t i = first; i < last; ++i) {
    Generators g;
    if (!g.Read(gens, LE16(bags, 4 * i), LE16(bags, 4 * (i + 1))))
      return false;
    out.push_back(g);
  }
  return true;
}
bool SF2(Bytes data, SoundBank& bank) {
  List root, sample_data, hydra;
  if (!Children(data, root) ||
      !Children(Find(root, "sdta", true), sample_data) ||
      !Children(Find(root, "pdta", true), hydra))
    return false;
  List info;
  if (!Children(Find(root, "INFO", true), info))
    return false;
  Bytes version = Find(info, "ifil");
  if (!version.empty() && (version.size() < 4 || LE16(version, 0) != 2))
    return false;
  Bytes smpl = Find(sample_data, "smpl"), phdr = Find(hydra, "phdr"),
        pbag = Find(hydra, "pbag"), pgen = Find(hydra, "pgen"),
        inst = Find(hydra, "inst"), ibag = Find(hydra, "ibag"),
        igen = Find(hydra, "igen"), shdr = Find(hydra, "shdr");
  if (smpl.empty() || smpl.size() % 2 || smpl.size() / 2 > kMaxSamples ||
      phdr.size() < 76 || phdr.size() % 38 || pbag.empty() || pbag.size() % 4 ||
      pgen.size() % 4 || inst.size() < 44 || inst.size() % 22 || ibag.empty() ||
      ibag.size() % 4 || igen.size() % 4 || shdr.size() < 92 ||
      shdr.size() % 46)
    return false;
  bank.samples.reserve(smpl.size() / 2);
  for (size_t i = 0; i < smpl.size(); i += 2)
    bank.samples.push_back(static_cast<int16_t>(LE16(smpl, i)) / 32768.0f);
  size_t budget = 262144;
  for (size_t p = 0; p + 38 < phdr.size(); p += 38) {
    const uint16_t preset = LE16(phdr, p + 20),
                   preset_bank = LE16(phdr, p + 22);
    if (preset > 127)
      return false;
    std::vector<Generators> presets;
    if (!Zones(pbag, pgen, LE16(phdr, p + 24), LE16(phdr, p + 62), presets,
               budget))
      return false;
    Generators pg;
    for (const auto& pz : presets) {
      if (!pz.set[41]) {
        pg = pz;
        continue;
      }
      Generators pr = Overlay(pg, pz);
      size_t i = size_t(pr.Get(41)) * 22;
      if (!Range(inst, i, 44))
        return false;
      std::vector<Generators> instruments;
      if (!Zones(ibag, igen, LE16(inst, i + 20), LE16(inst, i + 42),
                 instruments, budget))
        return false;
      Generators ig;
      for (const auto& iz : instruments) {
        if (!iz.set[53]) {
          ig = iz;
          continue;
        }
        Generators ir = Overlay(ig, iz);
        size_t s = size_t(ir.Get(53)) * 46;
        if (!Range(shdr, s, 92))
          return false;
        const uint16_t type = LE16(shdr, s + 44);
        if (type != 1 && type != 2 && type != 4 && type != 8)
          return false;  // No ROM or compressed SF3 samples.
        Region r;
        r.program = static_cast<uint8_t>(preset);
        r.bank = preset_bank;
        r.drums = preset_bank == 128;
        auto sum = [&](int op, int fallback = 0) {
          return ir.Get(op, fallback) + pr.Get(op);
        };
        auto address = [&](size_t at, int fine, int coarse, uint32_t& out) {
          int64_t n = int64_t(LE32(shdr, s + at)) + sum(fine) +
                      int64_t(sum(coarse)) * 32768;
          if (n < 0 || n > int64_t(bank.samples.size()))
            return false;
          out = static_cast<uint32_t>(n);
          return true;
        };
        if (!address(20, 0, 4, r.start) || !address(24, 1, 12, r.end) ||
            !address(28, 2, 45, r.loop_start) ||
            !address(32, 3, 50, r.loop_end))
          return false;
        r.rate = LE32(shdr, s + 36);
        r.root = ir.Get(58, shdr[s + 40] == 255 ? 60 : shdr[s + 40]);
        r.tune = sum(51) * 100.0f + sum(52) + static_cast<int8_t>(shdr[s + 41]);
        r.scale = static_cast<float>(sum(56, 100));
        r.pan = std::clamp(sum(17) / 500.0f, -1.0f, 1.0f);
        r.gain = Gain(-sum(48));
        r.attack = Timecents(sum(34, -12000));
        r.hold = Timecents(sum(35, -12000));
        r.decay = Timecents(sum(36, -12000));
        r.sustain = Gain(-std::max(0, sum(37)));
        r.release = Timecents(sum(38, -12000));
        r.loop = static_cast<uint8_t>(ir.Get(54) & 3);
        if (r.loop == 2)
          r.loop = 0;
        r.exclusive = ir.Get(57);
        for (int op : {43, 44}) {
          int a = ir.Get(op, 0x7f00), b = pr.Get(op, 0x7f00);
          int lo = std::max(a & 255, b & 255), hi = std::min(a >> 8, b >> 8);
          if (op == 43) {
            r.key_lo = lo;
            r.key_hi = hi;
          } else {
            r.vel_lo = lo;
            r.vel_hi = hi;
          }
        }
        // A preset can restrict an instrument to an empty key/velocity
        // intersection. That zone is silent, not a malformed soundfont.
        if (r.key_lo > r.key_hi || r.vel_lo > r.vel_hi)
          continue;
        if (!ValidRegion(r, bank.samples.size()) ||
            bank.regions.size() == kMaxRegions)
          return false;
        bank.regions.push_back(r);
      }
    }
  }
  return !bank.regions.empty();
}
}  // namespace

std::shared_ptr<const SoundBank> LoadSoundBank(Bytes bytes,
                                               std::string& error) {
  if (bytes.size() > 256 * 1024 * 1024) {
    error = "soundfont exceeds 256 MiB limit";
    return {};
  }
  Bytes body;
  auto bank = std::make_shared<SoundBank>();
  bool valid = false;
  if (RIFF(bytes, "DLS ", body))
    valid = DLS(body, *bank);
  else if (RIFF(bytes, "sfbk", body))
    valid = SF2(body, *bank);
  if (!valid) {
    error = "invalid or unsupported DLS/SF2 soundfont";
    return {};
  }
  return bank;
}
}  // namespace urge::audioformat
