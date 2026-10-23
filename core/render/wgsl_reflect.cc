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

#include "core/render/wgsl_reflect.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <regex>
#include <string>

#include "core/render/texture_format.h"

namespace urge::wgsl {

namespace {

struct TypeLayout {
  uint64_t size = 0;
  uint64_t align = 1;
};

uint64_t AlignUp(uint64_t value, uint64_t alignment) {
  if (alignment <= 1)
    return value;
  return ((value + alignment - 1) / alignment) * alignment;
}

std::string Trim(std::string_view text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])))
    ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
    --end;
  return std::string(text.substr(begin, end - begin));
}

std::string StripComments(std::string_view source) {
  std::string result;
  result.reserve(source.size());
  bool line_comment = false;
  for (size_t i = 0; i < source.size(); ++i) {
    const char c = source[i];
    if (line_comment) {
      if (c == '\n') {
        line_comment = false;
        result.push_back(c);
      }
      continue;
    }
    if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
      line_comment = true;
      ++i;
      continue;
    }
    result.push_back(c);
  }
  return result;
}

bool ContainsWord(const std::string& body, const std::string& word) {
  size_t position = body.find(word);
  while (position != std::string::npos) {
    const auto is_word = [](char c) {
      return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    };
    const bool left_ok = position == 0 || !is_word(body[position - 1]);
    const size_t tail = position + word.size();
    const bool right_ok = tail >= body.size() || !is_word(body[tail]);
    if (left_ok && right_ok)
      return true;
    position = body.find(word, position + 1);
  }
  return false;
}

std::string FunctionBody(const std::string& source,
                         const std::string& attribute) {
  const size_t start = source.find(attribute);
  if (start == std::string::npos)
    return {};

  size_t open = source.find('{', start);
  if (open == std::string::npos)
    return {};

  int depth = 0;
  for (size_t i = open; i < source.size(); ++i) {
    if (source[i] == '{')
      ++depth;
    else if (source[i] == '}') {
      --depth;
      if (depth == 0)
        return source.substr(open, i - open + 1);
    }
  }
  return {};
}

class Reflector {
 public:
  explicit Reflector(const std::string& source) : source_(source) {
    ParseStructs();
  }

  std::vector<std::pair<uint32_t, uint32_t>> Bindings() const { return order_; }

  std::vector<BindingInfo> Infos() const {
    std::vector<BindingInfo> result;
    result.reserve(order_.size());
    for (const auto& key : order_) {
      const auto it = infos_.find(key);
      if (it != infos_.end())
        result.push_back(it->second);
    }
    return result;
  }

  std::optional<util::LayoutEntry> Entry(uint32_t group,
                                         uint32_t binding) const {
    auto it = entries_.find({group, binding});
    if (it == entries_.end())
      return std::nullopt;
    return it->second;
  }

  void AddVertex(const std::string& body) { vertex_body_ = body; }
  void AddFragment(const std::string& body) { fragment_body_ = body; }

  void ParseBindings() {
    static const std::regex kBinding(
        R"(@group\((\d+)\)\s*@binding\((\d+)\)\s*var(?:<([^>]*)>)?\s+(\w+)\s*:\s*([^;]+);)");

    const auto begin =
        std::sregex_iterator(source_.begin(), source_.end(), kBinding);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
      const uint32_t group = static_cast<uint32_t>(std::stoul((*it)[1].str()));
      const uint32_t binding =
          static_cast<uint32_t>(std::stoul((*it)[2].str()));
      const std::string address = Trim((*it)[3].str());
      const std::string name = (*it)[4].str();
      const std::string type = Trim((*it)[5].str());

      const std::optional<util::LayoutEntry> entry =
          MakeEntry(group, binding, address, name, type);
      if (!entry.has_value())
        continue;

      auto key = std::make_pair(group, binding);
      auto found = entries_.find(key);
      if (found == entries_.end()) {
        entries_.emplace(key, *entry);

        BindingInfo info;
        info.group = group;
        info.binding = binding;
        info.name = name;
        info.kind = KindOf(address, type);
        info.size = SizeOf(type).size;
        infos_.emplace(key, std::move(info));

        order_.push_back(key);
      }
    }
  }

 private:
  std::optional<util::LayoutEntry> MakeEntry(uint32_t group,
                                             uint32_t binding,
                                             const std::string& address,
                                             const std::string& name,
                                             const std::string& type) {
    const wgpu::ShaderStage stage = StageOf(name);

    if (address == "uniform") {
      const TypeLayout layout = SizeOf(type);
      if (layout.size == 0)
        return std::nullopt;
      return util::LayoutEntry{binding, stage,
                               util::BufferLayout::Uniform(false, layout.size)};
    }

    if (address.rfind("storage", 0) == 0) {
      const TypeLayout layout = SizeOf(type);
      if (address.find("read_write") != std::string::npos)
        return util::LayoutEntry{
            binding, stage, util::BufferLayout::Storage(false, layout.size)};
      return util::LayoutEntry{
          binding, stage,
          util::BufferLayout::ReadOnlyStorage(false, layout.size)};
    }

    if (type == "sampler")
      return util::LayoutEntry{binding, stage,
                               util::SamplerLayout::Filtering()};
    if (type == "sampler_comparison")
      return util::LayoutEntry{binding, stage,
                               util::SamplerLayout::Comparison()};
    if (type.rfind("texture_depth", 0) == 0)
      return util::LayoutEntry{
          binding, stage,
          util::TextureLayout::Depth(wgpu::TextureViewDimension::e2D)};
    if (type.rfind("texture_storage", 0) == 0) {
      const size_t comma = type.find(',');
      const size_t open = type.find('<');
      if (open == std::string::npos || comma == std::string::npos)
        return std::nullopt;
      const std::string format_name =
          Trim(type.substr(open + 1, comma - open - 1));
      const wgpu::TextureFormat format = ParseTextureFormat(format_name);
      if (format == wgpu::TextureFormat::Undefined)
        return std::nullopt;
      return util::LayoutEntry{
          binding, stage,
          util::StorageTextureLayout::WriteOnly(format, DimensionOfType(type))};
    }
    if (type.rfind("texture_cube", 0) == 0)
      return util::LayoutEntry{
          binding, stage,
          util::TextureLayout::Float(wgpu::TextureViewDimension::Cube)};
    if (type.rfind("texture_2d_array", 0) == 0)
      return util::LayoutEntry{
          binding, stage,
          util::TextureLayout::Float(wgpu::TextureViewDimension::e2DArray)};
    if (type.rfind("texture_2d", 0) == 0)
      return util::LayoutEntry{
          binding, stage,
          util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)};

    (void)group;
    return std::nullopt;
  }

  static wgpu::TextureViewDimension DimensionOfType(const std::string& type) {
    if (type.rfind("texture_storage_2d_array", 0) == 0)
      return wgpu::TextureViewDimension::e2DArray;
    return wgpu::TextureViewDimension::e2D;
  }

  static BindingKind KindOf(const std::string& address,
                            const std::string& type) {
    if (address == "uniform")
      return BindingKind::kUniform;
    if (address.rfind("storage", 0) == 0)
      return address.find("read_write") != std::string::npos
                 ? BindingKind::kStorageReadWrite
                 : BindingKind::kStorage;
    if (type.rfind("texture_storage", 0) == 0)
      return BindingKind::kStorageTexture;
    if (type == "sampler")
      return BindingKind::kSampler;
    if (type == "sampler_comparison")
      return BindingKind::kSamplerComparison;
    if (type.rfind("texture_depth", 0) == 0)
      return BindingKind::kTextureDepth;
    if (type.rfind("texture_cube", 0) == 0)
      return BindingKind::kTextureCube;
    if (type.rfind("texture_2d_array", 0) == 0)
      return BindingKind::kTextureArray;
    return BindingKind::kTexture2D;
  }

  wgpu::ShaderStage StageOf(const std::string& name) const {
    uint32_t stage = 0;
    if (ContainsWord(vertex_body_, name))
      stage |= static_cast<uint32_t>(wgpu::ShaderStage::Vertex);
    if (ContainsWord(fragment_body_, name))
      stage |= static_cast<uint32_t>(wgpu::ShaderStage::Fragment);
    if (stage == 0)
      stage = static_cast<uint32_t>(wgpu::ShaderStage::Vertex) |
              static_cast<uint32_t>(wgpu::ShaderStage::Fragment);
    return static_cast<wgpu::ShaderStage>(stage);
  }

  void ParseStructs() {
    static const std::regex kStruct(R"(struct\s+(\w+)\s*\{([^}]*)\})");
    const auto begin =
        std::sregex_iterator(source_.begin(), source_.end(), kStruct);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it)
      structs_[(*it)[1].str()] = (*it)[2].str();
  }

  TypeLayout SizeOf(const std::string& raw_type) {
    const std::string type = Trim(raw_type);

    if (type.rfind("array<", 0) == 0) {
      const size_t comma = type.rfind(',');
      if (comma == std::string::npos)
        return {};
      const std::string inner = Trim(type.substr(6, comma - 6));
      const std::string count_text =
          Trim(type.substr(comma + 1, type.size() - comma - 2));
      const uint64_t count = std::stoull(count_text);
      const TypeLayout element = SizeOf(inner);
      if (element.size == 0)
        return {};
      const uint64_t stride = AlignUp(element.size, 16);
      return {stride * count, 16};
    }

    if (type == "f32" || type == "i32" || type == "u32" || type == "bool")
      return {4, 4};
    if (type == "vec2<f32>" || type == "vec2<i32>" || type == "vec2<u32>")
      return {8, 8};
    if (type == "vec3<f32>" || type == "vec3<i32>" || type == "vec3<u32>")
      return {12, 16};
    if (type == "vec4<f32>" || type == "vec4<i32>" || type == "vec4<u32>")
      return {16, 16};
    if (type == "mat4x4<f32>")
      return {64, 16};
    if (type == "mat3x3<f32>")
      return {48, 16};
    if (type == "mat2x2<f32>")
      return {16, 8};

    auto found = structs_.find(type);
    if (found == structs_.end())
      return {};

    TypeLayout layout;
    uint64_t offset = 0;
    std::string members = found->second;
    size_t start = 0;
    while (start < members.size()) {
      size_t comma = members.find(',', start);
      if (comma == std::string::npos)
        comma = members.size();
      const std::string member = Trim(members.substr(start, comma - start));
      start = comma + 1;
      if (member.empty())
        continue;

      const size_t colon = member.find(':');
      if (colon == std::string::npos)
        continue;

      const TypeLayout member_layout = SizeOf(member.substr(colon + 1));
      if (member_layout.size == 0)
        continue;

      offset = AlignUp(offset, member_layout.align);
      offset += member_layout.size;
      layout.align = std::max(layout.align, member_layout.align);
    }

    layout.size = AlignUp(offset, layout.align);
    return layout;
  }

  std::string source_;
  std::map<std::string, std::string> structs_;
  std::map<std::pair<uint32_t, uint32_t>, util::LayoutEntry> entries_;
  std::map<std::pair<uint32_t, uint32_t>, BindingInfo> infos_;
  std::vector<std::pair<uint32_t, uint32_t>> order_;
  std::string vertex_body_;
  std::string fragment_body_;
};

}

std::vector<BindingInfo> ReflectBindings(std::string_view vertex_source,
                                         std::string_view fragment_source) {
  const std::string vertex = StripComments(vertex_source);
  const std::string fragment = StripComments(fragment_source);

  Reflector reflector(vertex + "\n" + fragment);
  reflector.AddVertex(FunctionBody(vertex, "@vertex"));
  reflector.AddFragment(FunctionBody(fragment, "@fragment"));
  reflector.ParseBindings();
  return reflector.Infos();
}

std::vector<LayoutGroup> Reflect(std::string_view vertex_source,
                                 std::string_view fragment_source) {
  const std::string vertex = StripComments(vertex_source);
  const std::string fragment = StripComments(fragment_source);

  Reflector reflector(vertex + "\n" + fragment);
  reflector.AddVertex(FunctionBody(vertex, "@vertex"));
  reflector.AddFragment(FunctionBody(fragment, "@fragment"));
  reflector.ParseBindings();

  std::map<uint32_t, std::vector<util::LayoutEntry>> groups;
  for (const auto& key : reflector.Bindings()) {
    const std::optional<util::LayoutEntry> entry =
        reflector.Entry(key.first, key.second);
    if (entry.has_value())
      groups[key.first].push_back(*entry);
  }

  std::vector<LayoutGroup> result;
  result.reserve(groups.size());
  for (auto& [group, entries] : groups) {
    std::sort(
        entries.begin(), entries.end(),
        [](const util::LayoutEntry& left, const util::LayoutEntry& right) {
          return std::get<0>(left) < std::get<0>(right);
        });
    result.emplace_back(group, std::move(entries));
  }
  return result;
}

}
