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

namespace urge {

namespace {

constexpr wgpu::TextureFormat kShadowDepthFormat =
    wgpu::TextureFormat::Depth32Float;

}

RenderTarget3D::RenderTarget3D(int32_t width, int32_t height, bool with_color)
    : RenderTarget3D(width, height, with_color, 1) {}

RenderTarget3D::RenderTarget3D(int32_t width,
                               int32_t height,
                               bool with_color,
                               int32_t layers)
    : size_(std::max(1, width), std::max(1, height)),
      layers_(std::clamp(layers, 1, 8)) {
  CreateResources(size_.x, size_.y, with_color, layers_);
}

RenderTarget3D::~RenderTarget3D() = default;

void RenderTarget3D::CreateResources(int32_t width,
                                     int32_t height,
                                     bool with_color,
                                     int32_t layers) {
  const wgpu::Device device = GPUDevice::Get().device();

  if (with_color) {
    wgpu::TextureDescriptor color_desc;
    color_desc.usage = wgpu::TextureUsage::RenderAttachment |
                       wgpu::TextureUsage::TextureBinding |
                       wgpu::TextureUsage::CopySrc;
    color_desc.dimension = wgpu::TextureDimension::e2D;
    color_desc.size.width = static_cast<uint32_t>(width);
    color_desc.size.height = static_cast<uint32_t>(height);
    color_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    color_texture_ = device.CreateTexture(&color_desc);
    if (!color_texture_)
      throw Exception(Exception::kGPUError,
                      "render target 3d: the device rejected the color "
                      "texture.");
    color_view_ = color_texture_.CreateView(nullptr);
  }

  wgpu::TextureDescriptor depth_desc;
  depth_desc.usage = wgpu::TextureUsage::RenderAttachment |
                     wgpu::TextureUsage::TextureBinding;
  depth_desc.dimension = wgpu::TextureDimension::e2D;
  depth_desc.size.width = static_cast<uint32_t>(width);
  depth_desc.size.height = static_cast<uint32_t>(height);
  depth_desc.size.depthOrArrayLayers = static_cast<uint32_t>(layers);
  depth_desc.format = kShadowDepthFormat;
  depth_texture_ = device.CreateTexture(&depth_desc);
  if (!depth_texture_)
    throw Exception(Exception::kGPUError,
                    "render target 3d: the device rejected the depth texture.");

  wgpu::TextureViewDescriptor array_desc;
  array_desc.dimension = wgpu::TextureViewDimension::e2DArray;
  array_desc.baseArrayLayer = 0;
  array_desc.arrayLayerCount = static_cast<uint32_t>(layers);
  depth_array_view_ = depth_texture_.CreateView(&array_desc);
  if (!depth_array_view_)
    throw Exception(Exception::kGPUError,
                    "render target 3d: the device rejected the depth array "
                    "view.");

  if (layers == 6) {
    wgpu::TextureViewDescriptor cube_desc;
    cube_desc.dimension = wgpu::TextureViewDimension::Cube;
    cube_desc.baseArrayLayer = 0;
    cube_desc.arrayLayerCount = 6;
    depth_cube_view_ = depth_texture_.CreateView(&cube_desc);
    if (!depth_cube_view_)
      throw Exception(Exception::kGPUError,
                      "render target 3d: the device rejected the depth cube "
                      "view.");
  }

  layer_views_.resize(layers);
  for (int32_t layer = 0; layer < layers; ++layer) {
    wgpu::TextureViewDescriptor layer_desc;
    layer_desc.dimension = wgpu::TextureViewDimension::e2D;
    layer_desc.baseArrayLayer = static_cast<uint32_t>(layer);
    layer_desc.arrayLayerCount = 1;
    layer_views_[layer] = depth_texture_.CreateView(&layer_desc);
    if (!layer_views_[layer])
      throw Exception(Exception::kGPUError,
                      "render target 3d: the device rejected a depth layer "
                      "view.");
  }
}

wgpu::TextureView RenderTarget3D::depth_view(int32_t layer) const {
  if (layer < 0 || layer >= static_cast<int32_t>(layer_views_.size()))
    return nullptr;
  return layer_views_[layer];
}

wgpu::RenderPassEncoder RenderTarget3D::BeginRendering(
    wgpu::CommandEncoder encoder,
    std::optional<glm::vec4> clear,
    bool clear_depth,
    int32_t layer) {
  wgpu::RenderPassColorAttachment color_attachment = {};
  if (color_view_) {
    color_attachment.view = color_view_;
    color_attachment.loadOp = clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    if (clear) {
      color_attachment.clearValue.r = clear->r;
      color_attachment.clearValue.g = clear->g;
      color_attachment.clearValue.b = clear->b;
      color_attachment.clearValue.a = clear->a;
    }
  }

  wgpu::RenderPassDepthStencilAttachment depth_attachment = {};
  depth_attachment.view = depth_view(layer);
  depth_attachment.depthLoadOp =
      clear_depth ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
  depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
  depth_attachment.depthClearValue = 1.0f;

  wgpu::RenderPassDescriptor desc = {};
  if (color_view_) {
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &color_attachment;
  }
  desc.depthStencilAttachment = &depth_attachment;

  return encoder.BeginRenderPass(&desc);
}

}
