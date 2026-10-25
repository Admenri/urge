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

#include "core/scene3d/gltf_loader3d.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SDL3/SDL_iostream.h"
#include "SDL3_image/SDL_image.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include "core/canvas/bitmap.h"
#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/scene3d/mesh_renderer3d.h"
#include "core/system/filesystem.h"

namespace urge {

namespace {

struct Json {
  enum class Kind { kNull, kBool, kNumber, kString, kArray, kObject };

  Kind kind = Kind::kNull;
  bool boolean = false;
  double number = 0.0;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;

  bool IsNull() const { return kind == Kind::kNull; }
  bool IsNumber() const { return kind == Kind::kNumber; }
  bool IsString() const { return kind == Kind::kString; }
  bool IsArray() const { return kind == Kind::kArray; }
  size_t Size() const { return items.size(); }

  bool Has(const std::string& key) const {
    return fields.find(key) != fields.end();
  }

  const Json& operator[](const std::string& key) const {
    static const Json kEmpty;
    const auto it = fields.find(key);
    return it == fields.end() ? kEmpty : it->second;
  }

  const Json& At(size_t index) const {
    static const Json kEmpty;
    return index < items.size() ? items[index] : kEmpty;
  }

  double Number(double fallback = 0.0) const {
    return kind == Kind::kNumber ? number : fallback;
  }

  int Int(int fallback = 0) const {
    return kind == Kind::kNumber ? static_cast<int>(number) : fallback;
  }

  std::string String(std::string fallback = {}) const {
    return kind == Kind::kString ? text : std::move(fallback);
  }

  bool Bool(bool fallback = false) const {
    return kind == Kind::kBool ? boolean : fallback;
  }
};

int HexValue(char ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  return 0;
}

void AppendUtf8(uint32_t code, std::string* out) {
  if (code < 0x80) {
    out->push_back(static_cast<char>(code));
  } else if (code < 0x800) {
    out->push_back(static_cast<char>(0xC0 | (code >> 6)));
    out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xE0 | (code >> 12)));
    out->push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
  }
}

class JsonParser {
 public:
  explicit JsonParser(std::string_view text) : text_(text) {}

  Json Parse() {
    Skip();
    return ParseValue();
  }

 private:
  void Skip() {
    while (index_ < text_.size()) {
      const char ch = text_[index_];
      if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t')
        break;
      ++index_;
    }
  }

  char Peek() const { return index_ < text_.size() ? text_[index_] : '\0'; }

  Json ParseValue() {
    Skip();
    switch (Peek()) {
      case '{':
        return ParseObject();
      case '[':
        return ParseArray();
      case '"': {
        Json value;
        value.kind = Json::Kind::kString;
        value.text = ParseString();
        return value;
      }
      case 't': {
        index_ += 4;
        Json value;
        value.kind = Json::Kind::kBool;
        value.boolean = true;
        return value;
      }
      case 'f': {
        index_ += 5;
        Json value;
        value.kind = Json::Kind::kBool;
        value.boolean = false;
        return value;
      }
      case 'n': {
        index_ += 4;
        return Json{};
      }
      default:
        return ParseNumber();
    }
  }

  Json ParseObject() {
    Json value;
    value.kind = Json::Kind::kObject;
    ++index_;
    Skip();
    if (Peek() == '}') {
      ++index_;
      return value;
    }

    while (index_ < text_.size()) {
      Skip();
      std::string key = ParseString();
      Skip();
      if (Peek() == ':')
        ++index_;
      value.fields[std::move(key)] = ParseValue();
      Skip();
      const char ch = Peek();
      ++index_;
      if (ch == '}')
        break;
    }
    return value;
  }

  Json ParseArray() {
    Json value;
    value.kind = Json::Kind::kArray;
    ++index_;
    Skip();
    if (Peek() == ']') {
      ++index_;
      return value;
    }

    while (index_ < text_.size()) {
      value.items.push_back(ParseValue());
      Skip();
      const char ch = Peek();
      ++index_;
      if (ch == ']')
        break;
    }
    return value;
  }

  std::string ParseString() {
    std::string out;
    if (Peek() != '"')
      return out;
    ++index_;

    while (index_ < text_.size()) {
      const char ch = text_[index_++];
      if (ch == '"')
        break;
      if (ch != '\\') {
        out.push_back(ch);
        continue;
      }

      const char escape = text_[index_++];
      switch (escape) {
        case 'n':
          out.push_back('\n');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'u': {
          uint32_t code = 0;
          for (int i = 0; i < 4 && index_ < text_.size(); ++i)
            code = code * 16 + static_cast<uint32_t>(HexValue(text_[index_++]));
          AppendUtf8(code, &out);
          break;
        }
        default:
          out.push_back(escape);
          break;
      }
    }
    return out;
  }

  Json ParseNumber() {
    const size_t start = index_;
    while (index_ < text_.size()) {
      const char ch = text_[index_];
      if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' ||
          ch == 'e' || ch == 'E')
        ++index_;
      else
        break;
    }

    Json value;
    value.kind = Json::Kind::kNumber;
    if (index_ > start) {
      const std::string text(text_.substr(start, index_ - start));
      value.number = std::strtod(text.c_str(), nullptr);
    }
    return value;
  }

  std::string_view text_;
  size_t index_ = 0;
};

std::vector<uint8_t> ReadFile(URGE_EXCEPT, const std::string& path) {
  SDL_IOStream* stream = IOService::Get().OpenReadRaw(exception_state, path);
  if (!stream) {
    exception_state.Throw(ExceptionState::kRGSSError, "gltf: cannot open '{}'.",
                          path);
    return {};
  }

  const Sint64 size = SDL_GetIOSize(stream);
  std::vector<uint8_t> data(static_cast<size_t>(std::max<Sint64>(0, size)));
  if (!data.empty())
    SDL_ReadIO(stream, data.data(), data.size());
  SDL_CloseIO(stream);
  return data;
}

struct GlbFile {
  Json json;
  std::vector<uint8_t> bin;
};

GlbFile ParseGlb(URGE_EXCEPT,
                 const std::vector<uint8_t>& data,
                 const std::string& path) {
  if (data.size() < 12) {
    exception_state.Throw(ExceptionState::kRGSSError, "gltf: '{}' is too small.",
                          path);
    return {};
  }

  uint32_t magic = 0;
  uint32_t length = 0;
  std::memcpy(&magic, data.data(), sizeof(magic));
  std::memcpy(&length, data.data() + 8, sizeof(length));
  if (magic != 0x46546C67u) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "gltf: '{}' is not a binary glb container.", path);
    return {};
  }

  length = std::min<uint32_t>(length, static_cast<uint32_t>(data.size()));

  GlbFile file;
  size_t offset = 12;
  while (offset + 8 <= length) {
    uint32_t chunk_length = 0;
    uint32_t chunk_type = 0;
    std::memcpy(&chunk_length, data.data() + offset, sizeof(chunk_length));
    std::memcpy(&chunk_type, data.data() + offset + 4, sizeof(chunk_type));

    const size_t begin = offset + 8;
    const size_t end = std::min<size_t>(begin + chunk_length, length);
    if (end <= begin)
      break;

    if (chunk_type == 0x4E4F534Au) {
      file.json =
          JsonParser(std::string_view(
                         reinterpret_cast<const char*>(data.data() + begin),
                         end - begin))
              .Parse();
    } else if (chunk_type == 0x004E4942u) {
      file.bin.assign(data.begin() + begin, data.begin() + end);
    }

    offset = begin + chunk_length;
    offset = (offset + 3) & ~static_cast<size_t>(3);
  }

  if (file.json.IsNull()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "gltf: '{}' has no JSON chunk.", path);
    return {};
  }
  return file;
}

size_t ComponentSize(int type) {
  switch (type) {
    case 5120:
    case 5121:
      return 1;
    case 5122:
    case 5123:
      return 2;
    case 5125:
    case 5126:
      return 4;
    default:
      return 4;
  }
}

double ReadComponent(const uint8_t* ptr, int type) {
  switch (type) {
    case 5120: {
      int8_t value = 0;
      std::memcpy(&value, ptr, 1);
      return value;
    }
    case 5121:
      return *ptr;
    case 5122: {
      int16_t value = 0;
      std::memcpy(&value, ptr, 2);
      return value;
    }
    case 5123: {
      uint16_t value = 0;
      std::memcpy(&value, ptr, 2);
      return value;
    }
    case 5125: {
      uint32_t value = 0;
      std::memcpy(&value, ptr, 4);
      return value;
    }
    case 5126: {
      float value = 0.0f;
      std::memcpy(&value, ptr, 4);
      return value;
    }
    default:
      return 0.0;
  }
}

size_t ComponentCount(const std::string& type) {
  if (type == "VEC2")
    return 2;
  if (type == "VEC3")
    return 3;
  if (type == "VEC4")
    return 4;
  if (type == "MAT4")
    return 16;
  return 1;
}

struct AccessorData {
  std::vector<double> values;
  size_t count = 0;
  size_t components = 1;
};

AccessorData ReadAccessor(const GlbFile& glb, int index) {
  AccessorData result;
  if (index < 0 || static_cast<size_t>(index) >= glb.json["accessors"].Size())
    return result;

  const Json& accessor = glb.json["accessors"].At(static_cast<size_t>(index));
  result.count = static_cast<size_t>(accessor["count"].Int());
  result.components = ComponentCount(accessor["type"].String());

  const int component_type = accessor["componentType"].Int(5126);
  const int view_index = accessor["bufferView"].Int(-1);
  if (view_index < 0 || result.count == 0 || result.components == 0)
    return result;

  const Json& view =
      glb.json["bufferViews"].At(static_cast<size_t>(view_index));
  const size_t component_size = ComponentSize(component_type);
  const size_t base = static_cast<size_t>(view["byteOffset"].Int()) +
                      static_cast<size_t>(accessor["byteOffset"].Int());
  const size_t packed = result.components * component_size;
  const size_t stride =
      static_cast<size_t>(view["byteStride"].Int(static_cast<int>(packed)));

  const bool normalized = accessor["normalized"].Bool();
  double scale = 1.0;
  if (normalized) {
    switch (component_type) {
      case 5120:
        scale = 1.0 / 127.0;
        break;
      case 5121:
        scale = 1.0 / 255.0;
        break;
      case 5122:
        scale = 1.0 / 32767.0;
        break;
      case 5123:
        scale = 1.0 / 65535.0;
        break;
      default:
        break;
    }
  }

  result.values.reserve(result.count * result.components);
  for (size_t i = 0; i < result.count; ++i) {
    const size_t offset = base + i * stride;
    for (size_t c = 0; c < result.components; ++c) {
      const size_t at = offset + c * component_size;
      if (at + component_size > glb.bin.size()) {
        result.values.push_back(0.0);
        continue;
      }
      double value = ReadComponent(glb.bin.data() + at, component_type);
      if (normalized)
        value = std::max(value * scale, -1.0);
      result.values.push_back(value);
    }
  }
  return result;
}

constexpr std::string_view kLitVertexShader = R"(
@vertex
fn vs_main(@location(0) position: vec3<f32>,
           @location(1) normal: vec3<f32>,
           @location(2) uv: vec2<f32>) -> UrgeVSOut {
  let world = urge_world(vec4<f32>(position, 1.0));
  var out: UrgeVSOut;
  out.position = u_view.view_proj * world;
  out.world = world.xyz;
  out.normal = urge_world_normal(normal);
  out.uv = uv;
  return out;
}
)";

constexpr std::string_view kLitFragmentShader = R"(
struct MaterialParams {
  albedo: vec4<f32>,
  params: vec4<f32>,
};

@group(2) @binding(0) var<uniform> u_mat: MaterialParams;
@group(2) @binding(1) var u_albedo: texture_2d<f32>;
@group(2) @binding(2) var u_albedo_sampler: sampler;

@fragment
fn fs_main(in: UrgeVSOut) -> @location(0) vec4<f32> {
  let raw = textureSample(u_albedo, u_albedo_sampler, in.uv);
  let base = vec4<f32>(raw.rgb / max(raw.a, 0.0001), raw.a) * u_mat.albedo;
  let lit = urge_shade(in.world, normalize(in.normal), base.rgb,
                       u_mat.params.x, u_mat.params.y);
  return vec4<f32>(urge_linear_to_srgb(urge_aces_tonemap(lit)), base.a);
}
)";

constexpr std::string_view kUnlitFragmentShader = R"(
struct MaterialParams {
  albedo: vec4<f32>,
  params: vec4<f32>,
};

@group(2) @binding(0) var<uniform> u_mat: MaterialParams;
@group(2) @binding(1) var u_albedo: texture_2d<f32>;
@group(2) @binding(2) var u_albedo_sampler: sampler;

@fragment
fn fs_main(in: UrgeVSOut) -> @location(0) vec4<f32> {
  let raw = textureSample(u_albedo, u_albedo_sampler, in.uv);
  return vec4<f32>(raw.rgb / max(raw.a, 0.0001), raw.a) * u_mat.albedo;
}
)";

glm::mat4 NodeMatrix(const Json& node) {
  if (node["matrix"].IsArray() && node["matrix"].Size() == 16) {
    glm::mat4 matrix(1.0f);
    for (int column = 0; column < 4; ++column)
      for (int row = 0; row < 4; ++row)
        matrix[column][row] = static_cast<float>(
            node["matrix"].At(static_cast<size_t>(column * 4 + row)).Number());
    return matrix;
  }

  glm::vec3 translation(0.0f);
  if (node["translation"].IsArray())
    for (size_t i = 0; i < 3; ++i)
      translation[static_cast<int>(i)] =
          static_cast<float>(node["translation"].At(i).Number());

  glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
  if (node["rotation"].IsArray() && node["rotation"].Size() == 4) {
    rotation = glm::quat(static_cast<float>(node["rotation"].At(3).Number(1.0)),
                         static_cast<float>(node["rotation"].At(0).Number()),
                         static_cast<float>(node["rotation"].At(1).Number()),
                         static_cast<float>(node["rotation"].At(2).Number()));
    rotation = glm::normalize(rotation);
  }

  glm::vec3 scale(1.0f);
  if (node["scale"].IsArray())
    for (size_t i = 0; i < 3; ++i)
      scale[static_cast<int>(i)] =
          static_cast<float>(node["scale"].At(i).Number(1.0));

  return glm::translate(glm::mat4(1.0f), translation) *
         glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

struct Transform {
  glm::vec3 position = glm::vec3(0.0f);
  glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  glm::vec3 scale = glm::vec3(1.0f);
};

Transform Decompose(const glm::mat4& matrix) {
  Transform result;
  result.position = glm::vec3(matrix[3]);

  const glm::vec3 axis_x(matrix[0]);
  const glm::vec3 axis_y(matrix[1]);
  const glm::vec3 axis_z(matrix[2]);
  result.scale =
      glm::vec3(glm::length(axis_x), glm::length(axis_y), glm::length(axis_z));

  glm::mat3 basis(axis_x / std::max(result.scale.x, 1.0e-6f),
                  axis_y / std::max(result.scale.y, 1.0e-6f),
                  axis_z / std::max(result.scale.z, 1.0e-6f));
  result.rotation = glm::normalize(glm::quat_cast(basis));
  return result;
}

MaterialSource3D BuildMaterialSource(const GlbFile& glb,
                                     const std::vector<RefPtr<Bitmap>>& images,
                                     size_t index) {
  const Json& definition = glb.json["materials"].At(index);
  const Json& pbr = definition["pbrMetallicRoughness"];

  MaterialSource3D source;
  if (pbr["baseColorFactor"].IsArray())
    for (size_t i = 0; i < 4; ++i)
      source.color[static_cast<int>(i)] =
          static_cast<float>(pbr["baseColorFactor"].At(i).Number(1.0));

  source.roughness = static_cast<float>(pbr["roughnessFactor"].Number(0.7));
  source.metallic = static_cast<float>(pbr["metallicFactor"].Number(0.0));
  source.transparent = definition["alphaMode"].String() == "BLEND";

  const int texture_index = pbr["baseColorTexture"]["index"].Int(-1);
  if (texture_index >= 0 &&
      static_cast<size_t>(texture_index) < glb.json["textures"].Size()) {
    const int image_index =
        glb.json["textures"]
            .At(static_cast<size_t>(texture_index))["source"]
            .Int(-1);
    if (image_index >= 0 && static_cast<size_t>(image_index) < images.size())
      source.texture = images[static_cast<size_t>(image_index)];
  }
  return source;
}

RefPtr<Material3D> BuildMaterial(URGE_EXCEPT,
                                 const MaterialSource3D& source,
                                 bool unlit) {
  auto material = MakeRefCounted<Material3D>();
  RefPtr<MaterialPass3D> pass = material->AddPass(
      exception_state,
      source.transparent ? "transparent" : "opaque",
      std::string(kLitVertexShader),
      std::string(unlit ? kUnlitFragmentShader : kLitFragmentShader));

  pass->SetUniform(
      "u_mat", {source.color.r, source.color.g, source.color.b, source.color.a,
                source.roughness, source.metallic, 0.0f, 0.0f});
  pass->Attr_TagId(1);

  if (source.transparent) {
    pass->Attr_Queue(3000);
    pass->SetBlend("alpha");
    pass->Attr_DepthWrite(false);
  } else {
    pass->Attr_Queue(2000);
  }

  if (source.texture)
    pass->SetTexture("u_albedo", source.texture);
  return material;
}

RefPtr<Mesh3D> BuildMesh(URGE_EXCEPT,
                         const GlbFile& glb,
                         size_t mesh_index,
                         std::vector<SubMesh3D>* submeshes) {
  const Json& definition = glb.json["meshes"].At(mesh_index);
  const Json& primitives = definition["primitives"];

  std::vector<float> vertices;
  std::vector<uint32_t> indices;

  for (size_t p = 0; p < primitives.Size(); ++p) {
    const Json& primitive = primitives.At(p);
    const Json& attributes = primitive["attributes"];

    const AccessorData positions =
        ReadAccessor(glb, attributes["POSITION"].Int(-1));
    if (positions.count == 0)
      continue;

    const AccessorData normals =
        ReadAccessor(glb, attributes["NORMAL"].Int(-1));
    const AccessorData uvs =
        ReadAccessor(glb, attributes["TEXCOORD_0"].Int(-1));

    const size_t base_vertex = vertices.size() / 8;
    vertices.reserve(vertices.size() + positions.count * 8);
    for (size_t i = 0; i < positions.count; ++i) {
      const size_t offset = i * positions.components;
      vertices.push_back(static_cast<float>(positions.values[offset + 0]));
      vertices.push_back(static_cast<float>(positions.values[offset + 1]));
      vertices.push_back(static_cast<float>(positions.values[offset + 2]));

      if (normals.count > i && normals.components >= 3) {
        const size_t normal_offset = i * normals.components;
        vertices.push_back(
            static_cast<float>(normals.values[normal_offset + 0]));
        vertices.push_back(
            static_cast<float>(normals.values[normal_offset + 1]));
        vertices.push_back(
            static_cast<float>(normals.values[normal_offset + 2]));
      } else {
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
      }

      if (uvs.count > i && uvs.components >= 2) {
        const size_t uv_offset = i * uvs.components;
        vertices.push_back(static_cast<float>(uvs.values[uv_offset + 0]));
        vertices.push_back(static_cast<float>(uvs.values[uv_offset + 1]));
      } else {
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
      }
    }

    SubMesh3D submesh;
    submesh.index_offset = static_cast<uint32_t>(indices.size());

    const int indices_accessor = primitive["indices"].Int(-1);
    if (indices_accessor >= 0) {
      const AccessorData source = ReadAccessor(glb, indices_accessor);
      indices.reserve(indices.size() + source.count);
      for (size_t i = 0; i < source.count; ++i)
        indices.push_back(static_cast<uint32_t>(source.values[i]));
    } else {
      for (size_t i = 0; i < positions.count; ++i)
        indices.push_back(static_cast<uint32_t>(base_vertex + i));
    }

    submesh.index_count =
        static_cast<uint32_t>(indices.size() - submesh.index_offset);
    submesh.mat_slot =
        static_cast<uint32_t>(std::max(0, primitive["material"].Int(0)));
    if (submesh.index_count > 0)
      submeshes->push_back(submesh);
  }

  auto mesh = MakeRefCounted<Mesh3D>();
  mesh->SetVertices(exception_state, std::move(vertices));
  mesh->SetIndices(exception_state, std::move(indices));
  mesh->ClearSubmeshes();
  for (const SubMesh3D& submesh : *submeshes)
    mesh->AddSubmesh(exception_state, static_cast<int32_t>(submesh.index_offset),
                     static_cast<int32_t>(submesh.index_count),
                     static_cast<int32_t>(submesh.mat_slot));
  return mesh;
}

}

GLTFModel3D::GLTFModel3D(URGE_EXCEPT, std::string path) {
  Load(exception_state, std::move(path));
}

GLTFModel3D::~GLTFModel3D() = default;

void GLTFModel3D::Load(URGE_EXCEPT, std::string path) {
  const auto started = std::chrono::steady_clock::now();

  const GlbFile glb = ParseGlb(exception_state, ReadFile(exception_state, path),
                               path);
  if (exception_state.HadException())
    return;

  const Json& image_list = glb.json["images"];
  for (size_t i = 0; i < image_list.Size(); ++i) {
    const Json& image = image_list.At(i);
    const int view_index = image["bufferView"].Int(-1);
    if (view_index < 0) {
      images_.push_back(nullptr);
      continue;
    }

    const Json& view =
        glb.json["bufferViews"].At(static_cast<size_t>(view_index));
    const size_t offset = static_cast<size_t>(view["byteOffset"].Int());
    const size_t length = static_cast<size_t>(view["byteLength"].Int());
    if (length == 0 || offset + length > glb.bin.size()) {
      images_.push_back(nullptr);
      continue;
    }

    SDL_IOStream* stream = SDL_IOFromConstMem(glb.bin.data() + offset, length);
    SDL_Surface* surface = stream ? IMG_Load_IO(stream, true) : nullptr;
    images_.push_back(Bitmap::FromSurface(surface));
  }

  const Json& material_list = glb.json["materials"];
  for (size_t i = 0; i < material_list.Size(); ++i)
    material_sources_.push_back(BuildMaterialSource(glb, images_, i));
  if (material_sources_.empty())
    material_sources_.push_back(MaterialSource3D{});

  for (const MaterialSource3D& source : material_sources_)
    materials_.push_back(BuildMaterial(exception_state, source, unlit_));

  root_ = MakeRefCounted<Node3D>();

  const Json& meshes_json = glb.json["meshes"];
  meshes_.resize(meshes_json.Size());

  std::vector<RefPtr<Mesh3D>> built(meshes_json.Size());

  std::function<void(size_t, Node3D*)> build_node = [&](size_t node_index,
                                                        Node3D* parent) {
    const Json& node = glb.json["nodes"].At(node_index);

    RefPtr<Node3D> object;
    if (node["mesh"].IsNumber()) {
      const size_t mesh_index = static_cast<size_t>(node["mesh"].Int());
      if (mesh_index >= built.size()) {
        built.resize(mesh_index + 1);
      }
      if (!built[mesh_index]) {
        std::vector<SubMesh3D> submeshes;
        built[mesh_index] = BuildMesh(exception_state, glb, mesh_index, &submeshes);
        meshes_[mesh_index] = built[mesh_index];
      }

      auto renderer = MakeRefCounted<MeshRenderer3D>();
      renderer->Attr_Mesh(built[mesh_index]);
      const Json& primitives = glb.json["meshes"].At(mesh_index)["primitives"];
      for (size_t p = 0; p < primitives.Size(); ++p) {
        const int slot = std::max(0, primitives.At(p)["material"].Int(0));
        const size_t material_index = static_cast<size_t>(slot);
        renderer->SetMaterial(exception_state, slot,
                              material_index < materials_.size()
                                        ? materials_[material_index]
                                        : materials_.front());
      }
      object = renderer;
    } else {
      object = MakeRefCounted<Node3D>();
    }

    const Transform transform = Decompose(NodeMatrix(node));
    object->SetLocalPosition(transform.position);
    object->SetLocalRotation(
        glm::vec4(transform.rotation.x, transform.rotation.y,
                  transform.rotation.z, transform.rotation.w));
    object->SetLocalScale(transform.scale);
    parent->AddChild(object);
    ++node_count_;

    const Json& children = node["children"];
    for (size_t i = 0; i < children.Size(); ++i)
      build_node(static_cast<size_t>(children.At(i).Int()), object.get());
  };

  const Json& scenes = glb.json["scenes"];
  const size_t scene_index = static_cast<size_t>(glb.json["scene"].Int(0));
  const Json& nodes = scenes.At(scene_index)["nodes"];
  for (size_t i = 0; i < nodes.Size(); ++i)
    build_node(static_cast<size_t>(nodes.At(i).Int()), root_.get());

  glm::vec3 minimum(std::numeric_limits<float>::max());
  glm::vec3 maximum(std::numeric_limits<float>::lowest());
  bool has_bounds = false;

  std::function<void(Node3D*)> measure = [&](Node3D* node) {
    if (auto* renderer = dynamic_cast<MeshRenderer3D*>(node)) {
      RefPtr<Mesh3D> mesh = renderer->mesh();
      if (mesh) {
        const glm::vec3 center = mesh->bounds_center();
        const float radius = mesh->bounds_radius();
        const glm::mat4 world = node->world_matrix();
        for (int corner = 0; corner < 8; ++corner) {
          const glm::vec3 offset((corner & 1) ? radius : -radius,
                                 (corner & 2) ? radius : -radius,
                                 (corner & 4) ? radius : -radius);
          const glm::vec3 point =
              glm::vec3(world * glm::vec4(center + offset, 1.0f));
          minimum = glm::min(minimum, point);
          maximum = glm::max(maximum, point);
          has_bounds = true;
        }
      }
    }

    for (const RefPtr<Node3D>& child : node->children())
      measure(child.get());
  };

  measure(root_.get());

  if (!has_bounds) {
    minimum = glm::vec3(0.0f);
    maximum = glm::vec3(0.0f);
  }
  bounds_min_ = minimum;
  bounds_max_ = maximum;

  const double elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count();

  LOGGER_INFO(
      "gltf: '{}' -> {} nodes, {} meshes, {} materials, {} images in {:.1f} ms",
      path, node_count_, meshes_.size(), materials_.size(), images_.size(),
      elapsed);
}

std::vector<float> GLTFModel3D::BoundsCenter() const {
  const glm::vec3 center = (bounds_min_ + bounds_max_) * 0.5f;
  return {center.x, center.y, center.z};
}

std::vector<float> GLTFModel3D::BoundsSize() const {
  const glm::vec3 size = bounds_max_ - bounds_min_;
  return {size.x, size.y, size.z};
}

float GLTFModel3D::BoundsRadius() const {
  return glm::length(bounds_max_ - bounds_min_) * 0.5f;
}

void GLTFModel3D::Fit(float size) {
  if (!root_ || size <= 0.0f)
    return;

  const glm::vec3 extent = bounds_max_ - bounds_min_;
  const float longest = std::max(extent.x, std::max(extent.y, extent.z));
  if (longest <= 0.0f)
    return;

  const float scale = size / longest;
  const glm::vec3 center = (bounds_min_ + bounds_max_) * 0.5f;

  root_->SetLocalScale(glm::vec3(scale));
  root_->SetLocalPosition(-center * scale);
}

void GLTFModel3D::RebuildMaterials(URGE_EXCEPT) {
  if (material_sources_.empty() || !root_)
    return;

  for (size_t i = 0; i < material_sources_.size(); ++i) {
    if (i >= materials_.size())
      break;
    const RefPtr<Material3D> previous = materials_[i];
    const RefPtr<Material3D> replacement =
        BuildMaterial(exception_state, material_sources_[i], unlit_);
    materials_[i] = replacement;

    std::function<void(Node3D*)> rebind = [&](Node3D* node) {
      if (auto* renderer = dynamic_cast<MeshRenderer3D*>(node)) {
        std::vector<int32_t> slots;
        for (const auto& entry : renderer->materials())
          if (entry.second.get() == previous.get())
            slots.push_back(entry.first);
        for (const int32_t slot : slots)
          renderer->SetMaterial(exception_state, slot, replacement);
      }
      for (const RefPtr<Node3D>& child : node->children())
        rebind(child.get());
    };
    rebind(root_.get());
  }
}

void GLTFModel3D::SetMaterial(URGE_EXCEPT, int32_t index, RefPtr<Material3D> material) {
  if (index < 0 || static_cast<size_t>(index) >= materials_.size()) {
    exception_state.Throw(ExceptionState::kRGSSError, "gltf: material index {} is out of range (0..{}).", index,
                    static_cast<int32_t>(materials_.size()) - 1);
    return;
  }
  const RefPtr<Material3D> previous = materials_[static_cast<size_t>(index)];
  materials_[static_cast<size_t>(index)] = material;

  if (!root_ || previous.get() == material.get())
    return;

  std::function<void(Node3D*)> rebind = [&](Node3D* node) {
    if (auto* renderer = dynamic_cast<MeshRenderer3D*>(node)) {
      std::vector<int32_t> slots;
      for (const auto& entry : renderer->materials())
        if (entry.second.get() == previous.get())
          slots.push_back(entry.first);
      for (const int32_t slot : slots)
        renderer->SetMaterial(exception_state, slot, material);
    }

    for (const RefPtr<Node3D>& child : node->children())
      rebind(child.get());
  };
  rebind(root_.get());
}

RefPtr<Material3D> GLTFModel3D::GetMaterial(int32_t index) const {
  if (index < 0 || static_cast<size_t>(index) >= materials_.size())
    return nullptr;
  return materials_[static_cast<size_t>(index)];
}

}
