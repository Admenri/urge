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

#include "core/scene3d/material3d.h"

#include <algorithm>
#include <utility>

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/render/device.h"
#include "core/render/gpu_utils.h"
#include "core/render/shader.h"
#include "core/scene3d/lighting3d.h"

namespace urge {

namespace {

constexpr uint32_t kGroupView = 0;
constexpr uint32_t kGroupMesh = 1;
constexpr uint32_t kGroupMaterial = 2;
constexpr uint32_t kGroupGlobals = 3;
constexpr uint32_t kFirstReflectedGroup = 4;

using LayoutGroups =
    std::vector<std::pair<uint32_t, std::vector<util::LayoutEntry>>>;

class MaterialDefaults {
 public:
  static MaterialDefaults& Get() {
    static MaterialDefaults instance;
    return instance;
  }

  wgpu::TextureView white_view() const { return white_view_; }
  wgpu::Sampler sampler() const { return sampler_; }
  wgpu::Sampler repeat_sampler() const { return repeat_sampler_; }
  wgpu::Sampler shadow_sampler() const { return shadow_sampler_; }

 private:
  MaterialDefaults() {
    const wgpu::Device device = GPUDevice::Get().device();

    wgpu::TextureDescriptor texture_desc;
    texture_desc.usage =
        wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    texture_desc.dimension = wgpu::TextureDimension::e2D;
    texture_desc.size.width = 1;
    texture_desc.size.height = 1;
    texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    texture_ = device.CreateTexture(&texture_desc);

    const std::uint8_t white[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = texture_;

    wgpu::TexelCopyBufferLayout layout;
    layout.bytesPerRow = sizeof(white);
    layout.rowsPerImage = 1;

    wgpu::Extent3D size;
    size.width = 1;
    size.height = 1;
    size.depthOrArrayLayers = 1;
    GPUDevice::Get().queue().WriteTexture(&destination, white, sizeof(white),
                                          &layout, &size);

    white_view_ = texture_.CreateView(nullptr);

    wgpu::SamplerDescriptor sampler_desc;
    sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_desc.magFilter = wgpu::FilterMode::Linear;
    sampler_desc.minFilter = wgpu::FilterMode::Linear;
    sampler_ = device.CreateSampler(&sampler_desc);

    wgpu::SamplerDescriptor repeat_desc;
    repeat_desc.addressModeU = wgpu::AddressMode::Repeat;
    repeat_desc.addressModeV = wgpu::AddressMode::Repeat;
    repeat_desc.addressModeW = wgpu::AddressMode::Repeat;
    repeat_desc.magFilter = wgpu::FilterMode::Linear;
    repeat_desc.minFilter = wgpu::FilterMode::Linear;
    repeat_sampler_ = device.CreateSampler(&repeat_desc);

    wgpu::SamplerDescriptor shadow_desc;
    shadow_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    shadow_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    shadow_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    shadow_desc.magFilter = wgpu::FilterMode::Linear;
    shadow_desc.minFilter = wgpu::FilterMode::Linear;
    shadow_desc.compare = wgpu::CompareFunction::LessEqual;
    shadow_sampler_ = device.CreateSampler(&shadow_desc);
  }

  wgpu::Texture texture_;
  wgpu::TextureView white_view_;
  wgpu::Sampler sampler_;
  wgpu::Sampler repeat_sampler_;
  wgpu::Sampler shadow_sampler_;
};

uint64_t VertexFormatBytes(wgpu::VertexFormat format) {
  switch (format) {
    case wgpu::VertexFormat::Float32x2:
      return 8;
    case wgpu::VertexFormat::Float32x3:
      return 12;
    case wgpu::VertexFormat::Float32x4:
    case wgpu::VertexFormat::Uint32x4:
      return 16;
    default:
      return 16;
  }
}

wgpu::RenderPipeline BuildDepthOnlyPipeline(const LayoutGroups& groups,
                                            const std::string& vs_wgsl,
                                            uint32_t cull,
                                            bool skinned) {
  const wgpu::Device device = GPUDevice::Get().device();

  std::vector<wgpu::BindGroupLayout> layouts;
  layouts.reserve(groups.size());
  for (const auto& group : groups)
    layouts.push_back(util::CreateBindLayout(group.second));

  wgpu::PipelineLayoutDescriptor layout_desc;
  layout_desc.bindGroupLayoutCount = layouts.size();
  layout_desc.bindGroupLayouts = layouts.empty() ? nullptr : layouts.data();
  const wgpu::PipelineLayout pipeline_layout =
      device.CreatePipelineLayout(&layout_desc);

  wgpu::ShaderSourceWGSL source;
  source.code = wgpu::StringView(vs_wgsl);
  wgpu::ShaderModuleDescriptor module_desc;
  module_desc.nextInChain = &source;
  const wgpu::ShaderModule module = device.CreateShaderModule(&module_desc);

  wgpu::DepthStencilState depth;
  depth.format = wgpu::TextureFormat::Depth32Float;
  depth.depthWriteEnabled = wgpu::OptionalBool::True;
  depth.depthCompare = wgpu::CompareFunction::LessEqual;
  depth.depthBias = 2;
  depth.depthBiasSlopeScale = 2.0f;

  const std::vector<std::vector<VertexAttribute>> layout =
      skinned ? SkinnedMeshVertexLayout() : MeshVertexLayout();

  std::vector<wgpu::VertexAttribute> attributes;
  attributes.reserve(layout[0].size());
  uint64_t stride = 0;
  for (const VertexAttribute& attribute : layout[0]) {
    wgpu::VertexAttribute entry = {};
    entry.shaderLocation = attribute.location;
    entry.format = attribute.format;
    entry.offset = stride;
    attributes.push_back(entry);
    stride += VertexFormatBytes(attribute.format);
  }

  wgpu::VertexBufferLayout vertex_buffer = {};
  vertex_buffer.stepMode = wgpu::VertexStepMode::Vertex;
  vertex_buffer.arrayStride = stride;
  vertex_buffer.attributeCount = attributes.size();
  vertex_buffer.attributes = attributes.data();

  wgpu::RenderPipelineDescriptor desc;
  desc.layout = pipeline_layout;
  desc.vertex.module = module;
  desc.vertex.entryPoint = wgpu::StringView("vs_main");
  desc.vertex.bufferCount = 1;
  desc.vertex.buffers = &vertex_buffer;
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.primitive.cullMode = static_cast<wgpu::CullMode>(cull);
  desc.primitive.frontFace = wgpu::FrontFace::CCW;
  desc.depthStencil = &depth;

  return device.CreateRenderPipeline(&desc);
}

}

MaterialPass3D::MaterialPass3D(std::string tag,
                               std::string vs_wgsl,
                               std::string fs_wgsl)
    : tag_(std::move(tag)),
      vs_wgsl_(std::move(vs_wgsl)),
      fs_wgsl_(std::move(fs_wgsl)) {
  if (vs_wgsl_.empty())
    throw Exception(Exception::kRGSSError,
                    "material3d: a pass needs a vertex source.");
}

MaterialPass3D::~MaterialPass3D() = default;

void MaterialPass3D::CollectSlots() {
  slots_.clear();
  texture_names_.clear();
  uniform_names_.clear();

  if (fs_wgsl_.empty())
    return;

  const std::vector<wgsl::BindingInfo> infos =
      wgsl::ReflectBindings(vs_wgsl_, fs_wgsl_);

  for (const wgsl::BindingInfo& info : infos) {
    if (info.group != kGroupMaterial)
      continue;

    Slot slot;
    slot.name = info.name;
    slot.binding = info.binding;
    slot.kind = info.kind;
    slot.size = info.size;
    slots_.push_back(std::move(slot));
  }

  std::sort(slots_.begin(), slots_.end(),
            [](const Slot& left, const Slot& right) {
              return left.binding < right.binding;
            });

  for (const Slot& slot : slots_) {
    if (slot.kind == wgsl::BindingKind::kUniform)
      uniform_names_.push_back(slot.name);
    else if (slot.kind != wgsl::BindingKind::kSampler &&
             slot.kind != wgsl::BindingKind::kSamplerComparison)
      texture_names_.push_back(slot.name);
  }
}

void MaterialPass3D::SetTextureSlot(int32_t slot, RefPtr<Bitmap> texture) {
  if (slots_.empty())
    CollectSlots();
  if (texture_names_.empty())
    return;
  if (slot < 0 || slot >= static_cast<int32_t>(texture_names_.size()))
    throw Exception(Exception::kRGSSError,
                    "material3d: texture slot {} is out of range (0..{}).", slot,
                    static_cast<int32_t>(texture_names_.size()) - 1);

  texture_map_[texture_names_[slot]] = std::move(texture);
  binding_dirty_ = true;
}

RefPtr<Bitmap> MaterialPass3D::GetTextureSlot(int32_t slot) const {
  if (slot < 0 || slot >= static_cast<int32_t>(texture_names_.size()))
    return nullptr;

  const auto it = texture_map_.find(texture_names_[slot]);
  return it == texture_map_.end() ? nullptr : it->second;
}

void MaterialPass3D::SetTexture(std::string name, RefPtr<Bitmap> texture) {
  texture_map_[std::move(name)] = std::move(texture);
  binding_dirty_ = true;
}

RefPtr<Bitmap> MaterialPass3D::GetTexture(std::string name) const {
  const auto it = texture_map_.find(name);
  return it == texture_map_.end() ? nullptr : it->second;
}

void MaterialPass3D::SetUniformSlot(int32_t slot, std::vector<float> values) {
  if (slots_.empty())
    CollectSlots();
  if (uniform_names_.empty())
    return;
  if (slot < 0 || slot >= static_cast<int32_t>(uniform_names_.size()))
    throw Exception(Exception::kRGSSError,
                    "material3d: uniform slot {} is out of range (0..{}).", slot,
                    static_cast<int32_t>(uniform_names_.size()) - 1);

  uniform_map_[uniform_names_[slot]] = std::move(values);
  binding_dirty_ = true;
}

std::vector<float> MaterialPass3D::GetUniformSlot(int32_t slot) const {
  if (slot < 0 || slot >= static_cast<int32_t>(uniform_names_.size()))
    return {};

  const auto it = uniform_map_.find(uniform_names_[slot]);
  return it == uniform_map_.end() ? std::vector<float>() : it->second;
}

void MaterialPass3D::SetUniform(std::string name, std::vector<float> values) {
  uniform_map_[std::move(name)] = std::move(values);
  binding_dirty_ = true;
}

std::vector<float> MaterialPass3D::GetUniform(std::string name) const {
  const auto it = uniform_map_.find(name);
  return it == uniform_map_.end() ? std::vector<float>() : it->second;
}

std::vector<std::string> MaterialPass3D::SlotNames() const {
  std::vector<std::string> result;
  result.reserve(slots_.size());
  for (const Slot& slot : slots_)
    result.push_back(std::to_string(slot.binding) + ":" + slot.name);
  return result;
}

wgpu::RenderPipeline MaterialPass3D::AcquirePipeline() {
  if (pipeline_dirty_ || !holder_)
    RebuildPipeline();
  return holder_->pipeline;
}

wgpu::BindGroup MaterialPass3D::AcquireBindGroup() {
  if (binding_dirty_ || binding_ == nullptr)
    RebuildBindGroup();
  return binding_;
}

bool MaterialPass3D::HasGroup(uint32_t group) const {
  return group < group_count_;
}

void MaterialPass3D::RebuildPipeline() {
  using util::BufferLayout;
  using util::LayoutEntry;
  using util::SamplerLayout;
  using util::TextureLayout;

  const auto both_stages = static_cast<wgpu::ShaderStage>(
      static_cast<uint32_t>(wgpu::ShaderStage::Vertex) |
      static_cast<uint32_t>(wgpu::ShaderStage::Fragment));

  CollectSlots();

  LayoutGroups groups;
  groups.push_back(
      {kGroupView, depth_only_ ? ShadowViewEntries() : ViewEntries()});
  groups.push_back({kGroupMesh, MeshEntries()});

  if (depth_only_) {
    const wgpu::RenderPipeline pipeline =
        BuildDepthOnlyPipeline(groups, vs_wgsl_, cull_, skinned_);
    if (!pipeline)
      throw Exception(Exception::kGPUError,
                      "material3d: the device rejected the shadow pipeline.");

    auto holder = MakeRefCounted<PipelineHolder>();
    holder->pipeline = pipeline;

    holder_ = std::move(holder);
    group_count_ = static_cast<uint32_t>(groups.size());
    pipeline_dirty_ = false;
    binding_dirty_ = false;

    LOGGER_DEBUG("material3d: shadow pass built its pipeline");
    return;
  }

  if (fs_wgsl_.empty())
    throw Exception(Exception::kRGSSError,
                    "material3d: a lit pass needs a fragment source.");

  std::vector<LayoutEntry> material;
  material.reserve(slots_.size());
  for (const Slot& slot : slots_) {
    switch (slot.kind) {
      case wgsl::BindingKind::kUniform:
        material.push_back(
            {slot.binding, both_stages,
             BufferLayout::Uniform(false, std::max<uint64_t>(slot.size, 16))});
        break;
      case wgsl::BindingKind::kStorage:
        material.push_back({slot.binding, both_stages,
                            BufferLayout::ReadOnlyStorage(false, slot.size)});
        break;
      case wgsl::BindingKind::kTextureCube:
        material.push_back(
            {slot.binding, wgpu::ShaderStage::Fragment,
             TextureLayout::Float(wgpu::TextureViewDimension::Cube)});
        break;
      case wgsl::BindingKind::kTextureArray:
        material.push_back(
            {slot.binding, wgpu::ShaderStage::Fragment,
             TextureLayout::Float(wgpu::TextureViewDimension::e2DArray)});
        break;
      case wgsl::BindingKind::kTextureDepth:
        material.push_back(
            {slot.binding, wgpu::ShaderStage::Fragment,
             TextureLayout::Depth(wgpu::TextureViewDimension::e2D)});
        break;
      case wgsl::BindingKind::kSampler:
        material.push_back({slot.binding, wgpu::ShaderStage::Fragment,
                            SamplerLayout::Filtering()});
        break;
      case wgsl::BindingKind::kSamplerComparison:
        material.push_back({slot.binding, wgpu::ShaderStage::Fragment,
                            SamplerLayout::Comparison()});
        break;
      default:
        material.push_back(
            {slot.binding, wgpu::ShaderStage::Fragment,
             TextureLayout::Float(wgpu::TextureViewDimension::e2D)});
        break;
    }
  }
  groups.push_back({kGroupMaterial, std::move(material)});
  groups.push_back({kGroupGlobals, GlobalsEntries()});

  const std::vector<wgsl::LayoutGroup> reflected =
      wgsl::Reflect(vs_wgsl_, fs_wgsl_);
  uint32_t max_group = kFirstReflectedGroup - 1;
  for (const wgsl::LayoutGroup& group : reflected)
    max_group = std::max(max_group, group.first);

  for (uint32_t index = kFirstReflectedGroup; index <= max_group; ++index) {
    std::vector<LayoutEntry> entries;
    for (const wgsl::LayoutGroup& group : reflected)
      if (group.first == index)
        entries = group.second;
    groups.push_back({index, std::move(entries)});
  }

  const ShaderSource vertex{vs_wgsl_, "vs_main", groups};
  const ShaderSource fragment{fs_wgsl_, "fs_main", {}};
  Pipeline shader(vertex, fragment,
                  skinned_ ? SkinnedMeshVertexLayout() : MeshVertexLayout(),
                  {1});

  wgpu::PrimitiveState primitive = GetDefaultPrimitiveState();
  primitive.cullMode = static_cast<wgpu::CullMode>(cull_);
  primitive.frontFace = wgpu::FrontFace::CCW;

  wgpu::DepthStencilState depth;
  depth.format = wgpu::TextureFormat::Depth32Float;
  depth.depthWriteEnabled =
      depth_write_ ? wgpu::OptionalBool::True : wgpu::OptionalBool::False;
  depth.depthCompare = depth_test_ ? wgpu::CompareFunction::LessEqual
                                   : wgpu::CompareFunction::Always;
  depth.stencilFront.compare = wgpu::CompareFunction::Always;
  depth.stencilFront.failOp = wgpu::StencilOperation::Keep;
  depth.stencilFront.depthFailOp = wgpu::StencilOperation::Keep;
  depth.stencilFront.passOp = wgpu::StencilOperation::Keep;
  depth.stencilBack = depth.stencilFront;
  depth.stencilReadMask = 0;
  depth.stencilWriteMask = 0;

  const std::optional<wgpu::BlendState> blend = ParseBlendState(blend_);

  wgpu::ColorTargetState target;
  target.format = kColorTargetFormat;
  target.blend = blend ? &*blend : nullptr;

  const wgpu::RenderPipeline pipeline =
      shader.MakeState(primitive, depth, {target});

  auto holder = MakeRefCounted<PipelineHolder>();
  holder->pipeline = pipeline;

  holder_ = std::move(holder);
  group_count_ = static_cast<uint32_t>(groups.size());
  pipeline_dirty_ = false;
  binding_dirty_ = true;

  LOGGER_DEBUG("material3d: pass '{}' reflected {} slots", tag_,
               static_cast<int32_t>(slots_.size()));
}

void MaterialPass3D::RebuildBindGroup() {
  if (!holder_ || pipeline_dirty_)
    AcquirePipeline();

  std::vector<wgpu::BindGroupEntry> entries;
  entries.reserve(slots_.size());

  for (const Slot& slot : slots_) {
    wgpu::BindGroupEntry entry = {};
    entry.binding = slot.binding;

    switch (slot.kind) {
      case wgsl::BindingKind::kUniform: {
        const uint64_t size = std::max<uint64_t>(slot.size, 16);
        wgpu::Buffer& buffer = uniform_buffers_[slot.name];
        if (buffer == nullptr || buffer.GetSize() < size) {
          wgpu::BufferDescriptor desc;
          desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
          desc.size = size;
          buffer = GPUDevice::Get().device().CreateBuffer(&desc);
          if (buffer == nullptr)
            throw Exception(Exception::kGPUError,
                            "material3d: the device rejected a uniform buffer.");
        }

        std::vector<float> data(static_cast<size_t>(size / 4), 0.0f);
        const auto found = uniform_map_.find(slot.name);
        if (found != uniform_map_.end()) {
          const size_t count = std::min(found->second.size(), data.size());
          for (size_t i = 0; i < count; ++i)
            data[i] = found->second[i];
        }
        GPUDevice::Get().queue().WriteBuffer(buffer, 0, data.data(), size);

        entry.buffer = buffer;
        entry.offset = 0;
        entry.size = size;
        break;
      }
      case wgsl::BindingKind::kSampler:
        entry.sampler = MaterialDefaults::Get().repeat_sampler();
        break;
      case wgsl::BindingKind::kSamplerComparison:
        entry.sampler = MaterialDefaults::Get().shadow_sampler();
        break;
      default: {
        const auto found = texture_map_.find(slot.name);
        RefPtr<Bitmap> texture =
            found == texture_map_.end() ? nullptr : found->second;
        const bool usable = texture && !texture->IsDisposed();
        entry.textureView =
            usable ? texture->texture_view() : MaterialDefaults::Get().white_view();
        break;
      }
    }

    entries.push_back(entry);
  }

  wgpu::BindGroupDescriptor desc;
  desc.layout = holder_->pipeline.GetBindGroupLayout(kGroupMaterial);
  desc.entryCount = entries.size();
  desc.entries = entries.empty() ? nullptr : entries.data();

  binding_ = GPUDevice::Get().device().CreateBindGroup(&desc);
  if (binding_ == nullptr)
    throw Exception(Exception::kGPUError,
                    "material3d: the device rejected the material group.");
  binding_dirty_ = false;
}

Material3D::Material3D() = default;

Material3D::~Material3D() = default;

RefPtr<MaterialPass3D> Material3D::AddPass(std::string tag,
                                           std::string vs_wgsl,
                                           std::string fs_wgsl) {
  if (tag.empty())
    throw Exception(Exception::kRGSSError, "material3d: a pass needs a tag.");

  for (const RefPtr<MaterialPass3D>& pass : passes_) {
    if (pass->tag() == tag)
      throw Exception(Exception::kRGSSError,
                      "material3d: the pass tag '{}' is already used.", tag);
  }

  auto pass = MakeRefCounted<MaterialPass3D>(
      std::move(tag), std::move(vs_wgsl), std::move(fs_wgsl));
  passes_.push_back(pass);
  return pass;
}

RefPtr<MaterialPass3D> Material3D::AddShadowPass(std::string tag,
                                                 std::string vs_wgsl) {
  if (tag.empty())
    throw Exception(Exception::kRGSSError, "material3d: a pass needs a tag.");

  for (const RefPtr<MaterialPass3D>& pass : passes_) {
    if (pass->tag() == tag)
      throw Exception(Exception::kRGSSError,
                      "material3d: the pass tag '{}' is already used.", tag);
  }

  auto pass = MakeRefCounted<MaterialPass3D>(std::move(tag), std::move(vs_wgsl),
                                             std::string());
  pass->set_depth_only(true);
  passes_.push_back(pass);
  return pass;
}

RefPtr<MaterialPass3D> Material3D::GetPass(std::string tag) {
  for (const RefPtr<MaterialPass3D>& pass : passes_)
    if (pass->tag() == tag)
      return pass;
  return nullptr;
}

bool Material3D::HasPass(std::string tag) {
  return GetPass(std::move(tag)) != nullptr;
}

void Material3D::RemovePass(std::string tag) {
  passes_.erase(std::remove_if(passes_.begin(), passes_.end(),
                               [&tag](const RefPtr<MaterialPass3D>& pass) {
                                 return pass->tag() == tag;
                               }),
                passes_.end());
}

std::vector<std::string> Material3D::Tags() const {
  std::vector<std::string> tags;
  tags.reserve(passes_.size());
  for (const RefPtr<MaterialPass3D>& pass : passes_)
    tags.push_back(pass->tag());
  return tags;
}

}
