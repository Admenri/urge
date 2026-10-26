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

#include "core/scene3d/render_target3d.h"

#include <algorithm>
#include <utility>

#include "core/common/exception.h"
#include "core/render/device.h"
#include "core/render/texture_format.h"

namespace urge {

RenderTarget3D::RenderTarget3D(URGE_EXCEPT, int32_t width, int32_t height, bool with_color)
    : RenderTarget3D(exception_state, width, height, with_color, 1) {}

RenderTarget3D::RenderTarget3D(URGE_EXCEPT, int32_t width,
                               int32_t height,
                               bool with_color,
                               int32_t layers) {
  RenderTargetDesc3D desc;
  desc.width = width;
  desc.height = height;
  if (with_color)
    desc.colors.push_back(wgpu::TextureFormat::RGBA8Unorm);
  desc.depth = wgpu::TextureFormat::Depth32Float;
  desc.layers = layers;
  CreateResources(exception_state, desc);
}

RenderTarget3D::RenderTarget3D(URGE_EXCEPT, int32_t width,
                               int32_t height,
                               std::string color_format,
                               std::string depth_format,
                               int32_t samples) {
  RenderTargetDesc3D desc;
  desc.width = width;
  desc.height = height;
  desc.samples = static_cast<uint32_t>(std::max(1, samples));

  if (!color_format.empty()) {
    const wgpu::TextureFormat format = ParseTextureFormat(color_format);
    if (format == wgpu::TextureFormat::Undefined) {
      exception_state.Throw(ExceptionState::kRGSSError, "render target 3d: '{}' is not a texture format.",
                      color_format);
      return;
    }
    desc.colors.push_back(format);
  }

  if (!depth_format.empty()) {
    const wgpu::TextureFormat format = ParseTextureFormat(depth_format);
    if (format == wgpu::TextureFormat::Undefined || !IsDepthFormat(format)) {
      exception_state.Throw(ExceptionState::kRGSSError, "render target 3d: '{}' is not a depth format.",
                      depth_format);
      return;
    }
    desc.depth = format;
  } else {
    desc.depth = wgpu::TextureFormat::Undefined;
  }

  CreateResources(exception_state, desc);
}

RenderTarget3D::RenderTarget3D(URGE_EXCEPT, const RenderTargetDesc3D& desc) {
  CreateResources(exception_state, desc);
}

RenderTarget3D::~RenderTarget3D() = default;

void RenderTarget3D::CreateResources(URGE_EXCEPT, const RenderTargetDesc3D& desc) {
  const wgpu::Device device = GPUDevice::Get().device();

  size_ = glm::ivec2(std::max(1, desc.width), std::max(1, desc.height));
  layers_ = std::clamp(static_cast<int32_t>(desc.layers), 1, 8);
  samples_ = std::max<uint32_t>(1, desc.samples);
  const uint32_t mip_levels = std::max<uint32_t>(1, desc.mip_levels);

  for (const wgpu::TextureFormat format : desc.colors) {
    if (IsDepthFormat(format)) {
      exception_state.Throw(ExceptionState::kRGSSError, "render target 3d: '{}' is a depth format, not a color "
                      "format.",
                      TextureFormatName(format));
      return;
    }
    color_formats_.push_back(format);

    wgpu::TextureDescriptor color_desc;
    color_desc.usage = wgpu::TextureUsage::RenderAttachment;
    if (samples_ == 1)
      color_desc.usage |=
          wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc;
    if (desc.storage && IsStorageCapable(format))
      color_desc.usage |= wgpu::TextureUsage::StorageBinding;
    color_desc.dimension = wgpu::TextureDimension::e2D;
    color_desc.size.width = static_cast<uint32_t>(size_.x);
    color_desc.size.height = static_cast<uint32_t>(size_.y);
    color_desc.size.depthOrArrayLayers = 1;
    color_desc.format = format;
    color_desc.sampleCount = samples_;
    color_desc.mipLevelCount = mip_levels;

    wgpu::Texture texture = device.CreateTexture(&color_desc);
    if (!texture) {
      exception_state.Throw(ExceptionState::kGPUError, "render target 3d: the device rejected the color "
                      "texture.");
      return;
    }
    color_textures_.push_back(texture);
    color_views_.push_back(texture.CreateView(nullptr));

    if (samples_ > 1) {
      wgpu::TextureDescriptor resolve_desc;
      resolve_desc.usage = wgpu::TextureUsage::RenderAttachment |
                           wgpu::TextureUsage::TextureBinding |
                           wgpu::TextureUsage::CopySrc;
      resolve_desc.dimension = wgpu::TextureDimension::e2D;
      resolve_desc.size.width = static_cast<uint32_t>(size_.x);
      resolve_desc.size.height = static_cast<uint32_t>(size_.y);
      resolve_desc.size.depthOrArrayLayers = 1;
      resolve_desc.format = format;
      resolve_desc.mipLevelCount = mip_levels;

      wgpu::Texture resolved = device.CreateTexture(&resolve_desc);
      if (!resolved) {
        exception_state.Throw(ExceptionState::kGPUError, "render target 3d: the device rejected the resolve "
                        "texture.");
        return;
      }
      color_textures_.push_back(resolved);
      resolve_views_.push_back(resolved.CreateView(nullptr));
    } else {
      resolve_views_.push_back(color_views_.back());
    }
  }

  depth_format_ = desc.depth;
  if (depth_format_ == wgpu::TextureFormat::Undefined)
    return;

  wgpu::TextureDescriptor depth_desc;
  depth_desc.usage = wgpu::TextureUsage::RenderAttachment;
  if (samples_ == 1)
    depth_desc.usage |= wgpu::TextureUsage::TextureBinding;
  depth_desc.dimension = wgpu::TextureDimension::e2D;
  depth_desc.size.width = static_cast<uint32_t>(size_.x);
  depth_desc.size.height = static_cast<uint32_t>(size_.y);
  depth_desc.size.depthOrArrayLayers = static_cast<uint32_t>(layers_);
  depth_desc.format = depth_format_;
  depth_desc.sampleCount = samples_;

  depth_texture_ = device.CreateTexture(&depth_desc);
  if (!depth_texture_) {
    exception_state.Throw(ExceptionState::kGPUError, "render target 3d: the device rejected the depth texture.");
    return;
  }
  wgpu::TextureViewDescriptor array_desc;
  array_desc.dimension = wgpu::TextureViewDimension::e2DArray;
  array_desc.baseArrayLayer = 0;
  array_desc.arrayLayerCount = static_cast<uint32_t>(layers_);
  depth_array_view_ = depth_texture_.CreateView(&array_desc);
  if (!depth_array_view_) {
    exception_state.Throw(ExceptionState::kGPUError, "render target 3d: the device rejected the depth array "
                    "view.");
    return;
  }
  if (layers_ == 6) {
    wgpu::TextureViewDescriptor cube_desc;
    cube_desc.dimension = wgpu::TextureViewDimension::Cube;
    cube_desc.baseArrayLayer = 0;
    cube_desc.arrayLayerCount = 6;
    depth_cube_view_ = depth_texture_.CreateView(&cube_desc);
    if (!depth_cube_view_) {
      exception_state.Throw(ExceptionState::kGPUError, "render target 3d: the device rejected the depth cube "
                      "view.");
      return;
    }
  }

  layer_views_.resize(layers_);
  for (int32_t layer = 0; layer < layers_; ++layer) {
    wgpu::TextureViewDescriptor layer_desc;
    layer_desc.dimension = wgpu::TextureViewDimension::e2D;
    layer_desc.baseArrayLayer = static_cast<uint32_t>(layer);
    layer_desc.arrayLayerCount = 1;
    layer_views_[layer] = depth_texture_.CreateView(&layer_desc);
    if (!layer_views_[layer]) {
      exception_state.Throw(ExceptionState::kGPUError, "render target 3d: the device rejected the depth layer "
                      "view.");
      return;
    }
  }
}

bool RenderTarget3D::HasStencil() const {
  return IsStencilFormat(depth_format_);
}

std::vector<std::string> RenderTarget3D::ColorFormats() const {
  std::vector<std::string> result;
  result.reserve(color_formats_.size());
  for (const wgpu::TextureFormat format : color_formats_)
    result.push_back(TextureFormatName(format));
  return result;
}

std::string RenderTarget3D::DepthFormat() const {
  return TextureFormatName(depth_format_);
}

wgpu::TextureView RenderTarget3D::color_view(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(color_views_.size()))
    return nullptr;
  return color_views_[index];
}

wgpu::TextureView RenderTarget3D::sampled_view(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(resolve_views_.size()))
    return nullptr;
  return resolve_views_[index];
}

wgpu::TextureView RenderTarget3D::depth_view(int32_t layer) const {
  if (layer < 0 || layer >= static_cast<int32_t>(layer_views_.size()))
    return nullptr;
  return layer_views_[layer];
}

wgpu::RenderPassEncoder RenderTarget3D::BeginRendering(
    wgpu::CommandEncoder encoder,
    const std::vector<std::optional<glm::vec4>>& clears,
    bool clear_depth,
    bool clear_stencil,
    int32_t layer) {
  std::vector<wgpu::RenderPassColorAttachment> attachments;
  attachments.reserve(color_views_.size());

  for (size_t index = 0; index < color_views_.size(); ++index) {
    const std::optional<glm::vec4>& clear =
        index < clears.size() ? clears[index] : std::nullopt;

    wgpu::RenderPassColorAttachment attachment = {};
    attachment.view = color_views_[index];
    attachment.resolveTarget = samples_ > 1 ? resolve_views_[index] : nullptr;
    attachment.loadOp = clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
    attachment.storeOp = wgpu::StoreOp::Store;
    if (clear) {
      attachment.clearValue.r = clear->r;
      attachment.clearValue.g = clear->g;
      attachment.clearValue.b = clear->b;
      attachment.clearValue.a = clear->a;
    }
    attachments.push_back(attachment);
  }

  wgpu::RenderPassDepthStencilAttachment depth_attachment = {};
  const bool has_depth = depth_texture_ != nullptr;
  if (has_depth) {
    const int32_t clamped =
        std::clamp(layer, 0, static_cast<int32_t>(layer_views_.size()) - 1);
    depth_attachment.view = layer_views_[clamped];
    depth_attachment.depthLoadOp =
        clear_depth ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
    depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
    depth_attachment.depthClearValue = 1.0f;

    if (IsStencilFormat(depth_format_)) {
      depth_attachment.stencilLoadOp =
          clear_stencil ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
      depth_attachment.stencilStoreOp = wgpu::StoreOp::Store;
      depth_attachment.stencilClearValue = 0;
    }
  }

  wgpu::RenderPassDescriptor desc = {};
  if (!attachments.empty()) {
    desc.colorAttachmentCount = attachments.size();
    desc.colorAttachments = attachments.data();
  }
  if (has_depth)
    desc.depthStencilAttachment = &depth_attachment;

  return encoder.BeginRenderPass(&desc);
}

}
