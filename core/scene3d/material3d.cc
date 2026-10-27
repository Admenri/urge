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
#include "core/graphics/gpu.h"
#include "core/render/device.h"
#include "core/render/gpu_utils.h"
#include "core/render/texture_format.h"
#include "core/scene3d/lighting3d.h"
#include "core/scene3d/shader_lib3d.h"

namespace urge {

namespace {

constexpr uint32_t kGroupView = 0;
constexpr uint32_t kGroupMesh = 1;
constexpr uint32_t kGroupMaterial = 2;
constexpr uint32_t kGroupGlobals = 3;

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

wgpu::StencilFaceState MakeStencilFace(uint32_t compare,
                                       uint32_t fail,
                                       uint32_t depth_fail,
                                       uint32_t pass) {
  wgpu::StencilFaceState state;
  state.compare = static_cast<wgpu::CompareFunction>(compare);
  state.failOp = static_cast<wgpu::StencilOperation>(fail);
  state.depthFailOp = static_cast<wgpu::StencilOperation>(depth_fail);
  state.passOp = static_cast<wgpu::StencilOperation>(pass);
  return state;
}

}

std::string PipelineTargetState3D::Key() const {
  std::string key;
  key.reserve(colors.size() * 4 + 16);
  for (const wgpu::TextureFormat format : colors) {
    key += std::to_string(static_cast<uint32_t>(format));
    key.push_back(',');
  }
  key.push_back('|');
  key += std::to_string(static_cast<uint32_t>(depth));
  key.push_back('|');
  key += std::to_string(samples);
  key.push_back('|');
  key += std::to_string(cull);
  key.push_back(flip_winding ? 'f' : 'n');
  return key;
}

MaterialPass3D::MaterialPass3D(URGE_EXCEPT,
                               std::string tag,
                               std::string vs_wgsl,
                               std::string fs_wgsl)
    : tag_(std::move(tag)),
      vs_wgsl_(ComposeShader3D(vs_wgsl)),
      fs_wgsl_(fs_wgsl.empty() ? std::string() : ComposeShader3D(fs_wgsl)) {
  if (vs_wgsl_.empty()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "material3d: a pass needs a vertex source.");
    return;
  }
}

MaterialPass3D::~MaterialPass3D() = default;

void MaterialPass3D::CollectSlots(URGE_EXCEPT) {
  slots_.clear();
  texture_names_.clear();
  uniform_names_.clear();

  if (fs_wgsl_.empty())
    return;

  const std::vector<wgsl::BindingInfo> infos =
      wgsl::ReflectBindings(vs_wgsl_, fs_wgsl_);

  for (const wgsl::BindingInfo& info : infos) {
    if (info.group > kGroupGlobals) {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "material3d: the pass '{}' declares group({}); a "
                            "3D material may only use groups 0..3.",
                            tag_, info.group);
      return;
    }
    if (info.group != kGroupMaterial)
      continue;
    if (info.kind == wgsl::BindingKind::kStorage ||
        info.kind == wgsl::BindingKind::kStorageReadWrite ||
        info.kind == wgsl::BindingKind::kStorageTexture) {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "material3d: the pass '{}' binds '{}' as a "
                            "storage resource in group 2; storage buffers and "
                            "storage textures belong to a compute pass.",
                            tag_, info.name);
      return;
    }

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

void MaterialPass3D::SetTextureSlot(URGE_EXCEPT,
                                    int32_t slot,
                                    RefPtr<Bitmap> texture) {
  if (slots_.empty()) {
    CollectSlots(exception_state);
    if (exception_state.HadException())
      return;
  }
  if (texture_names_.empty())
    return;
  if (slot < 0 || slot >= static_cast<int32_t>(texture_names_.size())) {
    exception_state.Throw(
        ExceptionState::kRGSSError,
        "material3d: texture slot {} is out of range (0..{}).", slot,
        static_cast<int32_t>(texture_names_.size()) - 1);
    return;
  }
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

void MaterialPass3D::SetTextureView(std::string name,
                                    RefPtr<GPUTextureView> view) {
  const auto found = view_map_.find(name);
  if (found != view_map_.end() && found->second.get() == view.get())
    return;
  view_map_[std::move(name)] = std::move(view);
  binding_dirty_ = true;
}

RefPtr<Bitmap> MaterialPass3D::GetTexture(std::string name) const {
  const auto it = texture_map_.find(name);
  return it == texture_map_.end() ? nullptr : it->second;
}

void MaterialPass3D::SetUniformSlot(URGE_EXCEPT,
                                    int32_t slot,
                                    std::vector<float> values) {
  if (slots_.empty()) {
    CollectSlots(exception_state);
    if (exception_state.HadException())
      return;
  }
  if (uniform_names_.empty())
    return;
  if (slot < 0 || slot >= static_cast<int32_t>(uniform_names_.size())) {
    exception_state.Throw(
        ExceptionState::kRGSSError,
        "material3d: uniform slot {} is out of range (0..{}).", slot,
        static_cast<int32_t>(uniform_names_.size()) - 1);
    return;
  }
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

std::vector<std::vector<VertexAttribute>> MaterialPass3D::VertexLayout() const {
  if (fullscreen_)
    return {};
  if (instanced_)
    return skinned_ ? InstancedSkinnedMeshVertexLayout()
                    : InstancedMeshVertexLayout();
  return skinned_ ? SkinnedMeshVertexLayout() : MeshVertexLayout();
}

void MaterialPass3D::EnsureBuilder(URGE_EXCEPT) {
  if (builder_)
    return;

  CollectSlots(exception_state);
  if (exception_state.HadException())
    return;

  const auto both_stages = static_cast<wgpu::ShaderStage>(
      static_cast<uint32_t>(wgpu::ShaderStage::Vertex) |
      static_cast<uint32_t>(wgpu::ShaderStage::Fragment));

  std::vector<util::LayoutEntry> material;
  material.reserve(slots_.size());
  for (const Slot& slot : slots_) {
    switch (slot.kind) {
      case wgsl::BindingKind::kUniform:
        material.push_back({slot.binding, both_stages,
                            util::BufferLayout::Uniform(
                                false, std::max<uint64_t>(slot.size, 16))});
        break;
      case wgsl::BindingKind::kTextureCube:
        material.push_back(
            {slot.binding, wgpu::ShaderStage::Fragment,
             util::TextureLayout::Float(wgpu::TextureViewDimension::Cube)});
        break;
      case wgsl::BindingKind::kTextureArray:
        material.push_back(
            {slot.binding, wgpu::ShaderStage::Fragment,
             util::TextureLayout::Float(wgpu::TextureViewDimension::e2DArray)});
        break;
      case wgsl::BindingKind::kTextureDepth:
        material.push_back(
            {slot.binding, wgpu::ShaderStage::Fragment,
             util::TextureLayout::Depth(wgpu::TextureViewDimension::e2D)});
        break;
      case wgsl::BindingKind::kSampler:
        material.push_back({slot.binding, wgpu::ShaderStage::Fragment,
                            util::SamplerLayout::Filtering()});
        break;
      case wgsl::BindingKind::kSamplerComparison:
        material.push_back({slot.binding, wgpu::ShaderStage::Fragment,
                            util::SamplerLayout::Comparison()});
        break;
      default:
        material.push_back(
            {slot.binding, wgpu::ShaderStage::Fragment,
             util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)});
        break;
    }
  }

  material_layout_ = util::CreateBindLayout(material);

  LayoutGroups groups;
  groups.push_back({kGroupView, ViewEntries()});
  groups.push_back({kGroupMesh, fullscreen_ ? std::vector<util::LayoutEntry>()
                                            : MeshEntries()});
  groups.push_back({kGroupMaterial, std::move(material)});
  groups.push_back({kGroupGlobals, GlobalsEntries()});

  const ShaderSource vertex{vs_wgsl_, vertex_entry_, groups};
  const ShaderSource fragment{fs_wgsl_, fragment_entry_, {}};

  builder_ = std::make_unique<Pipeline>(vertex, fragment, VertexLayout(),
                                        std::set<uint32_t>{kGroupMesh},
                                        MeshStepModes(instanced_));
  group_count_ = static_cast<uint32_t>(groups.size());
}

void MaterialPass3D::EnsureShadowBuilder() {
  if (shadow_builder_)
    return;

  LayoutGroups groups;
  groups.push_back({kGroupView, ShadowViewEntries()});
  groups.push_back({kGroupMesh, MeshEntries()});

  const ShaderSource vertex{vs_wgsl_, vertex_entry_, groups};
  const ShaderSource fragment{};

  shadow_builder_ = std::make_unique<Pipeline>(vertex, fragment, VertexLayout(),
                                               std::set<uint32_t>{kGroupView},
                                               MeshStepModes(instanced_));
}

wgpu::DepthStencilState MaterialPass3D::MakeDepthState(
    wgpu::TextureFormat format) const {
  wgpu::DepthStencilState state;
  state.format = format;
  state.depthWriteEnabled =
      depth_write_ ? wgpu::OptionalBool::True : wgpu::OptionalBool::False;
  state.depthCompare = depth_test_
                           ? static_cast<wgpu::CompareFunction>(depth_compare_)
                           : wgpu::CompareFunction::Always;
  state.depthBias = depth_bias_;
  state.depthBiasSlopeScale = depth_bias_slope_;
  state.depthBiasClamp = depth_bias_clamp_;

  if (stencil_ && IsStencilFormat(format)) {
    state.stencilFront =
        MakeStencilFace(stencil_front_compare_, stencil_front_fail_,
                        stencil_front_depth_fail_, stencil_front_pass_);
    state.stencilBack =
        MakeStencilFace(stencil_back_compare_, stencil_back_fail_,
                        stencil_back_depth_fail_, stencil_back_pass_);
    state.stencilReadMask = static_cast<uint32_t>(stencil_read_mask_);
    state.stencilWriteMask = static_cast<uint32_t>(stencil_write_mask_);
  } else {
    state.stencilFront = MakeStencilFace(8, 0, 0, 0);
    state.stencilBack = MakeStencilFace(8, 0, 0, 0);
    state.stencilReadMask = 0;
    state.stencilWriteMask = 0;
  }

  return state;
}

wgpu::RenderPipeline MaterialPass3D::AcquirePipeline(
    URGE_EXCEPT,
    const PipelineTargetState3D& target) {
  if (state_dirty_) {
    pipelines_.clear();
    shadow_pipelines_.clear();
    state_dirty_ = false;
  }

  const std::string key = target.Key();
  for (const auto& entry : pipelines_)
    if (entry.first == key)
      return entry.second;

  EnsureBuilder(exception_state);

  const std::optional<wgpu::BlendState> blend =
      ParseBlendState(exception_state, blend_);

  std::vector<wgpu::ColorTargetState> colors;
  colors.reserve(target.colors.size());
  for (const wgpu::TextureFormat format : target.colors) {
    wgpu::ColorTargetState state;
    state.format = format;
    state.blend = blend ? &*blend : nullptr;
    state.writeMask = static_cast<wgpu::ColorWriteMask>(color_write_mask_);
    colors.push_back(state);
  }

  wgpu::PrimitiveState primitive = GetDefaultPrimitiveState();
  primitive.topology = static_cast<wgpu::PrimitiveTopology>(topology_);
  primitive.cullMode = static_cast<wgpu::CullMode>(
      target.cull >= 0 ? target.cull : static_cast<int32_t>(cull_));
  primitive.frontFace =
      target.flip_winding
          ? (static_cast<wgpu::FrontFace>(front_face_) == wgpu::FrontFace::CCW
                 ? wgpu::FrontFace::CW
                 : wgpu::FrontFace::CCW)
          : static_cast<wgpu::FrontFace>(front_face_);

  std::optional<wgpu::DepthStencilState> depth;
  if (target.depth != wgpu::TextureFormat::Undefined)
    depth = MakeDepthState(target.depth);

  wgpu::MultisampleState samples;
  samples.count = target.samples;
  samples.alphaToCoverageEnabled = alpha_to_coverage_;

  const wgpu::RenderPipeline pipeline =
      builder_->MakeState(primitive, depth, colors, samples);

  pipelines_.emplace_back(key, pipeline);
  binding_dirty_ = true;
  return pipeline;
}

wgpu::RenderPipeline MaterialPass3D::AcquireDepthOnlyPipeline(
    const PipelineTargetState3D& target) {
  if (state_dirty_) {
    pipelines_.clear();
    shadow_pipelines_.clear();
    state_dirty_ = false;
  }

  const std::string key = target.Key();
  for (const auto& entry : shadow_pipelines_)
    if (entry.first == key)
      return entry.second;

  EnsureShadowBuilder();

  wgpu::DepthStencilState depth;
  depth.format = target.depth == wgpu::TextureFormat::Undefined
                     ? wgpu::TextureFormat::Depth32Float
                     : target.depth;
  depth.depthWriteEnabled = wgpu::OptionalBool::True;
  depth.depthCompare = wgpu::CompareFunction::LessEqual;
  depth.depthBias = depth_bias_ != 0 ? depth_bias_ : 2;
  depth.depthBiasSlopeScale =
      depth_bias_slope_ != 0.0f ? depth_bias_slope_ : 2.0f;
  depth.depthBiasClamp = depth_bias_clamp_;
  depth.stencilFront = MakeStencilFace(8, 0, 0, 0);
  depth.stencilBack = MakeStencilFace(8, 0, 0, 0);
  depth.stencilReadMask = 0;
  depth.stencilWriteMask = 0;

  wgpu::PrimitiveState primitive = GetDefaultPrimitiveState();
  primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  primitive.cullMode = static_cast<wgpu::CullMode>(cull_);
  primitive.frontFace =
      target.flip_winding
          ? (static_cast<wgpu::FrontFace>(front_face_) == wgpu::FrontFace::CCW
                 ? wgpu::FrontFace::CW
                 : wgpu::FrontFace::CCW)
          : static_cast<wgpu::FrontFace>(front_face_);

  wgpu::MultisampleState samples;
  samples.count = target.samples;

  const wgpu::RenderPipeline pipeline =
      shadow_builder_->MakeState(primitive, depth, {}, samples);

  shadow_pipelines_.emplace_back(key, pipeline);
  return pipeline;
}

wgpu::BindGroup MaterialPass3D::AcquireBindGroup(URGE_EXCEPT) {
  if (binding_dirty_ || binding_ == nullptr)
    RebuildBindGroup(exception_state);
  return binding_;
}

bool MaterialPass3D::HasGroup(uint32_t group) const {
  return group < group_count_;
}

void MaterialPass3D::RebuildBindGroup(URGE_EXCEPT) {
  EnsureBuilder(exception_state);

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
          if (buffer == nullptr) {
            exception_state.Throw(
                ExceptionState::kGPUError,
                "material3d: the device rejected a uniform buffer.");
            return;
          }
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
        const auto view = view_map_.find(slot.name);
        if (view != view_map_.end() && view->second)
          entry.textureView = view->second->handle();

        if (entry.textureView == nullptr) {
          const auto found = texture_map_.find(slot.name);
          RefPtr<Bitmap> texture =
              found == texture_map_.end() ? nullptr : found->second;
          const bool usable = texture && !texture->IsDisposed();
          entry.textureView = usable ? texture->texture_view()
                                     : MaterialDefaults::Get().white_view();
        }
        break;
      }
    }

    entries.push_back(entry);
  }

  wgpu::BindGroupDescriptor desc;
  desc.layout = material_layout_;
  desc.entryCount = entries.size();
  desc.entries = entries.empty() ? nullptr : entries.data();

  binding_ = GPUDevice::Get().device().CreateBindGroup(&desc);
  if (binding_ == nullptr) {
    exception_state.Throw(
        ExceptionState::kGPUError,
        "material3d: the device rejected the material group.");
    return;
  }
  binding_dirty_ = false;
}

Material3D::Material3D() = default;

Material3D::~Material3D() = default;

RefPtr<MaterialPass3D> Material3D::AddPass(URGE_EXCEPT,
                                           std::string tag,
                                           std::string vs_wgsl,
                                           std::string fs_wgsl) {
  if (tag.empty()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "material3d: a pass needs a tag.");
    return {};
  }
  for (const RefPtr<MaterialPass3D>& pass : passes_) {
    if (pass->tag() == tag) {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "material3d: the pass tag '{}' is already used.",
                            tag);
      return {};
    }
  }

  auto pass = MakeRefCounted<MaterialPass3D>(
      exception_state, std::move(tag), std::move(vs_wgsl), std::move(fs_wgsl));
  passes_.push_back(pass);
  return pass;
}

RefPtr<MaterialPass3D> Material3D::AddShadowPass(URGE_EXCEPT,
                                                 std::string tag,
                                                 std::string vs_wgsl) {
  RefPtr<MaterialPass3D> pass = AddPass(exception_state, std::move(tag),
                                        std::move(vs_wgsl), std::string());
  pass->set_depth_only(true);
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

RefPtr<MaterialPass3D> Material3D::PassAt(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(passes_.size()))
    return nullptr;
  return passes_[index];
}

}
