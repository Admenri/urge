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

#include "core/bitmap.h"

#include <algorithm>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "SDL3_image/SDL_image.h"
#include "glm/ext/matrix_clip_space.hpp"

#include "core/filesystem.h"
#include "core/gpu_utils.h"
#include "core/pipeline.h"

namespace urge {

static const SDL_PixelFormat kInternalPixelFormat = SDL_PIXELFORMAT_ABGR8888;

namespace {

glm::vec4 PremultiplyColor(const glm::vec4& color) {
  glm::vec4 result = color / 255.0f;
  result.r *= result.a;
  result.g *= result.a;
  result.b *= result.a;
  return result;
}

glm::vec4 UnpremultiplyColor(const std::uint8_t pixel[4]) {
  const float alpha = static_cast<float>(pixel[3]);
  if (alpha <= 0.0f)
    return glm::vec4(0.0f);

  return glm::vec4(
      std::min(255.0f, static_cast<float>(pixel[0]) * 255.0f / alpha),
      std::min(255.0f, static_cast<float>(pixel[1]) * 255.0f / alpha),
      std::min(255.0f, static_cast<float>(pixel[2]) * 255.0f / alpha), alpha);
}

glm::vec4 PremultiplyOpacity(int32_t opacity) {
  const float value =
      std::clamp(static_cast<float>(opacity) / 255.0f, 0.0f, 1.0f);
  return glm::vec4(value, value, value, value);
}

RectI AlignTextRect(const RectI& region,
                    int32_t text_width,
                    int32_t text_height,
                    int32_t align) {
  int32_t x = region.x;
  switch (align) {
    case 1:
      x += (region.width - text_width) / 2;
      break;
    case 2:
      x += region.width - text_width;
      break;
    default:
      break;
  }

  const int32_t y = region.y + (region.height - text_height) / 2;
  return RectI(x, y, text_width, text_height);
}

void UnpremultiplyPixelRow(std::uint8_t* pixels, int32_t width) {
  for (int32_t x = 0; x < width; ++x, pixels += 4) {
    const std::uint32_t alpha = pixels[3];

    if (alpha == 0) {
      pixels[0] = 0;
      pixels[1] = 0;
      pixels[2] = 0;
    } else if (alpha != 0xFF) {
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const std::uint32_t value =
            (static_cast<std::uint32_t>(pixels[channel]) * 255 + alpha / 2) /
            alpha;
        pixels[channel] = static_cast<std::uint8_t>(std::min(255u, value));
      }
    }
  }
}

struct MapResult {
  bool invoked = false;
  WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
  std::string error;
};

std::string_view DescribeMapStatus(const MapResult& mapping) {
  if (!mapping.invoked)
    return "the mapping callback was never invoked";

  switch (mapping.status) {
    case WGPUMapAsyncStatus_Success:
      return "the mapping succeeded but the buffer is not usable";
    case WGPUMapAsyncStatus_CallbackCancelled:
      return "the mapping was cancelled";
    case WGPUMapAsyncStatus_Error:
      return "the mapping failed with an error";
    case WGPUMapAsyncStatus_Aborted:
      return "the mapping was aborted, the device is probably lost";
    default:
      return "the mapping ended with an unknown status";
  }
}

std::vector<std::uint8_t> ReadTextureRegion(wgpu::Texture texture,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height) {
  if (width <= 0 || height <= 0)
    return {};

  auto& gpu = GPUDevice::Get();

  constexpr std::uint32_t kPixelSize = 4;
  constexpr std::uint32_t kRowAlignment = 256;

  const std::uint32_t pixel_pitch =
      static_cast<std::uint32_t>(width) * kPixelSize;
  const std::uint32_t row_pitch =
      (pixel_pitch + kRowAlignment - 1) / kRowAlignment * kRowAlignment;
  const std::size_t byte_size = static_cast<std::size_t>(row_pitch) * height;

  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = byte_size;
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  auto staging = gpu.device().CreateBuffer(&buffer_desc);

  wgpu::TexelCopyTextureInfo source;
  source.texture = texture;
  source.aspect = wgpu::TextureAspect::All;
  source.origin.x = static_cast<std::uint32_t>(x);
  source.origin.y = static_cast<std::uint32_t>(y);

  wgpu::TexelCopyBufferInfo destination;
  destination.buffer = staging;
  destination.layout.bytesPerRow = row_pitch;
  destination.layout.rowsPerImage = static_cast<std::uint32_t>(height);

  wgpu::Extent3D copy_size;
  copy_size.width = static_cast<std::uint32_t>(width);
  copy_size.height = static_cast<std::uint32_t>(height);

  auto encoder = gpu.device().CreateCommandEncoder(nullptr);
  encoder.CopyTextureToBuffer(&source, &destination, &copy_size);

  auto command = encoder.Finish(nullptr);
  gpu.queue().Submit(1, &command);

  MapResult mapping;
  WGPUBufferMapCallbackInfo map_callback = {};
  map_callback.mode = WGPUCallbackMode_WaitAnyOnly;
  map_callback.callback = [](WGPUMapAsyncStatus status, WGPUStringView message,
                             void* userdata1, void*) {
    auto* result = static_cast<MapResult*>(userdata1);
    result->invoked = true;
    result->status = status;
    if (status != WGPUMapAsyncStatus_Success)
      result->error = std::string(wgpu::StringView(message));
  };
  map_callback.userdata1 = &mapping;

  auto future =
      staging.MapAsync(wgpu::MapMode::Read, 0, byte_size, map_callback);

  gpu.WaitAny(future);

  if (mapping.status != WGPUMapAsyncStatus_Success) {
    throw Exception(Exception::kRGSSError,
                    "failed to map the staging buffer of a texture read back, "
                    "{}: {}.",
                    DescribeMapStatus(mapping),
                    mapping.error.empty()
                        ? std::string_view("the backend gave no reason")
                        : std::string_view(mapping.error));
  }

  const auto* mapped_range = static_cast<const std::uint8_t*>(
      staging.GetConstMappedRange(0, byte_size));
  if (!mapped_range) {
    throw Exception(Exception::kRGSSError,
                    "the staging buffer of a texture read back is mapped but "
                    "does not expose a range of {} bytes.",
                    byte_size);
  }

  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(pixel_pitch) *
                                   height);
  for (int32_t row = 0; row < height; ++row)
    std::memcpy(pixels.data() + static_cast<std::size_t>(row) * pixel_pitch,
                mapped_range + static_cast<std::size_t>(row) * row_pitch,
                pixel_pitch);

  staging.Unmap();
  return pixels;
}

}  // namespace

Bitmap::Bitmap(std::string filename) : font_(MakeRefCounted<Font>()) {
  SDL_Surface* image = nullptr;

  IOService::Get().OpenRead(
      filename, [&](SDL_IOStream* stream, const std::string& ext) {
        image = IMG_LoadTyped_IO(stream, true, ext.c_str());
        return !!image;
      });

  if (image->format != kInternalPixelFormat) {
    auto* converted_image = SDL_ConvertSurface(image, kInternalPixelFormat);
    SDL_DestroySurface(image);
    image = converted_image;
  }

  CreateInternal(image);
}

Bitmap::Bitmap(int32_t width, int32_t height) : font_(MakeRefCounted<Font>()) {
  SDL_Surface* image = SDL_CreateSurface(width, height, kInternalPixelFormat);

  CreateInternal(image);
}

Bitmap::Bitmap(RefPtr<Bitmap> other)
    : Bitmap(other->size().x, other->size().y) {
  auto encoder = g_device.CreateCommandEncoder(nullptr);

  wgpu::TexelCopyTextureInfo source, destination;
  source.texture = other->texture();
  destination.texture = texture();

  wgpu::Extent3D copy_size;
  copy_size.width = other->size().x;
  copy_size.height = other->size().y;

  encoder.CopyTextureToTexture(&source, &destination, &copy_size);

  auto command = encoder.Finish(nullptr);
  g_queue.Submit(1, &command);
}

Bitmap::~Bitmap() {
  Disposable::Dispose();
}

int32_t Bitmap::Width() {
  return size_.x;
}

int32_t Bitmap::Height() {
  return size_.y;
}

RefPtr<Rect> Bitmap::GetRect() {
  return MakeRefCounted<Rect>(0, 0, size_.x, size_.y);
}

void Bitmap::Blt(int32_t x,
                 int32_t y,
                 RefPtr<Bitmap> src_bitmap,
                 RefPtr<Rect> src_rect,
                 int32_t opacity) {
  Disposable::Guard();

  if (!src_rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  StretchBlt(
      MakeRefCounted<Rect>(x, y, src_rect->data.width, src_rect->data.height),
      src_bitmap, src_rect, opacity);
}

void Bitmap::StretchBlt(RefPtr<Rect> dst_rect,
                        RefPtr<Bitmap> src_bitmap,
                        RefPtr<Rect> src_rect,
                        int32_t opacity) {
  Disposable::Guard();

  if (!dst_rect || !src_bitmap || !src_rect)
    throw Exception(Exception::kRGSSError, "invalid rect or bitmap value.");

  const std::uint32_t vertex_count =
      primitive_
          .EmitQuad(dst_rect->data, MakeNorm(src_rect->data, src_bitmap->size_),
                    glm::vec4(opacity / 255.0f))
          .Upload();

  auto encoder = g_device.CreateCommandEncoder(nullptr);
  auto pass = BeginRendering(encoder);
  {
    auto pipeline = ShaderSet::Get().state.bitmap.texture_pma;
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, scene_group_, 0, nullptr);
    pass.SetBindGroup(1, object_group_, 0, nullptr);
    pass.SetBindGroup(2, src_bitmap->texture_group_, 0, nullptr);
    pass.SetVertexBuffer(0, primitive_.buffer(), 0, WGPU_WHOLE_SIZE);
    pass.Draw(vertex_count, 1, 0, 0);
  }
  pass.End();
  auto command = encoder.Finish(nullptr);
  g_queue.Submit(1, &command);
}

void Bitmap::FillRect(int32_t x,
                      int32_t y,
                      int32_t width,
                      int32_t height,
                      RefPtr<Color> color) {
  Disposable::Guard();

  if (!color)
    throw Exception(Exception::kRGSSError, "invalid color value.");

  GradientFillRect(x, y, width, height, color, color);
}

void Bitmap::FillRect(RefPtr<Rect> rect, RefPtr<Color> color) {
  Disposable::Guard();

  if (!rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  FillRect(rect->data.x, rect->data.y, rect->data.width, rect->data.height,
           color);
}

void Bitmap::GradientFillRect(int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              RefPtr<Color> color1,
                              RefPtr<Color> color2,
                              bool vertical) {
  Disposable::Guard();

  if (!color1 || !color2)
    throw Exception(Exception::kRGSSError, "invalid color value.");

  auto color1_norm = PremultiplyColor(color1->data);
  auto color2_norm = PremultiplyColor(color2->data);

  if (vertical)
    primitive_.EmitQuad(RectI(x, y, width, height), RectF(), color1_norm,
                        color1_norm, color2_norm, color2_norm);
  else
    primitive_.EmitQuad(RectI(x, y, width, height), RectF(), color1_norm,
                        color2_norm, color1_norm, color2_norm);

  const std::uint32_t vertex_count = primitive_.Upload();

  auto encoder = g_device.CreateCommandEncoder(nullptr);
  auto pass = BeginRendering(encoder);
  {
    auto pipeline = ShaderSet::Get().state.bitmap.color_noblend;
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, scene_group_, 0, nullptr);
    pass.SetBindGroup(1, object_group_, 0, nullptr);
    pass.SetVertexBuffer(0, primitive_.buffer(), 0, WGPU_WHOLE_SIZE);
    pass.Draw(vertex_count, 1, 0, 0);
  }
  pass.End();
  auto command = encoder.Finish(nullptr);
  g_queue.Submit(1, &command);
}

void Bitmap::GradientFillRect(RefPtr<Rect> rect,
                              RefPtr<Color> color1,
                              RefPtr<Color> color2,
                              bool vertical) {
  Disposable::Guard();

  if (!rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  GradientFillRect(rect->data.x, rect->data.y, rect->data.width,
                   rect->data.height, color1, color2, vertical);
}

void Bitmap::Clear() {
  Disposable::Guard();

  ClearRect(0, 0, size_.x, size_.y);
}

void Bitmap::ClearRect(int32_t x, int32_t y, int32_t width, int32_t height) {
  Disposable::Guard();

  FillRect(x, y, width, height, MakeRefCounted<Color>());
}

void Bitmap::ClearRect(RefPtr<Rect> rect) {
  Disposable::Guard();

  if (!rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  ClearRect(rect->data.x, rect->data.y, rect->data.width, rect->data.height);
}

RefPtr<Color> Bitmap::GetPixel(int32_t x, int32_t y) {
  Disposable::Guard();

  if (x < 0 || y < 0 || x >= size_.x || y >= size_.y)
    return MakeRefCounted<Color>();

  const std::vector<std::uint8_t> pixels =
      ReadTextureRegion(texture_, x, y, 1, 1);

  const glm::vec4 color = UnpremultiplyColor(pixels.data());
  return MakeRefCounted<Color>(color.r, color.g, color.b, color.a);
}

void Bitmap::SetPixel(int32_t x, int32_t y, RefPtr<Color> color) {
  GradientFillRect(x, y, 1, 1, color, color);
}

void Bitmap::HueChange(int32_t hue) {
  Disposable::Guard();
}

void Bitmap::Blur() {
  Disposable::Guard();
}

void Bitmap::RadialBlur(int32_t angle, int32_t division) {
  Disposable::Guard();
}

void Bitmap::DrawText(int32_t x,
                      int32_t y,
                      int32_t width,
                      int32_t height,
                      std::string str,
                      int32_t align) {
  Disposable::Guard();

  if (!font_ || str.empty())
    return;

  if (width <= 0 || height <= 0)
    return;

  FontRenderer::TextRun* run = text_renderer_.Acquire(*font_, str);
  if (!run)
    return;

  const RectI composed =
      AlignTextRect(RectI(x, y, width, height), run->size.x, run->size.y, align);
  const RectI blit_region =
      MakeIntersect(composed, RectI(0, 0, size_.x, size_.y));
  if (!blit_region.width || !blit_region.height)
    return;

  const RectF source_rect(static_cast<float>(blit_region.x - composed.x),
                          static_cast<float>(blit_region.y - composed.y),
                          static_cast<float>(blit_region.width),
                          static_cast<float>(blit_region.height));

  const glm::vec4 opacity = PremultiplyOpacity(run->opacity);

  const std::uint32_t vertex_count =
      primitive_
          .EmitQuad(RectF(blit_region),
                    MakeNorm(source_rect, glm::vec2(run->size)), opacity)
          .Upload();

  auto encoder = g_device.CreateCommandEncoder(nullptr);
  auto pass = BeginRendering(encoder);
  {
    pass.SetPipeline(ShaderSet::Get().state.bitmap.texture_pma);
    pass.SetBindGroup(0, scene_group_, 0, nullptr);
    pass.SetBindGroup(1, object_group_, 0, nullptr);
    pass.SetBindGroup(2, run->group, 0, nullptr);
    pass.SetVertexBuffer(0, primitive_.buffer(), 0, WGPU_WHOLE_SIZE);
    pass.Draw(vertex_count, 1, 0, 0);
  }
  pass.End();
  auto command = encoder.Finish(nullptr);
  g_queue.Submit(1, &command);
}

void Bitmap::DrawText(RefPtr<Rect> rect, std::string str, int32_t align) {
  Disposable::Guard();

  if (!rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  DrawText(rect->data.x, rect->data.y, rect->data.width, rect->data.height, str,
           align);
}

RefPtr<Rect> Bitmap::TextSize(std::string str) {
  Disposable::Guard();

  if (!font_)
    return MakeRefCounted<Rect>();

  int32_t width = 0, height = 0;
  if (!font_->MeasureText(str, &width, &height))
    return MakeRefCounted<Rect>();

  return MakeRefCounted<Rect>(0, 0, width, height);
}

RefPtr<Palette> Bitmap::ToPalette() {
  Disposable::Guard();

  auto* data = SDL_CreateSurface(size_.x, size_.y, kInternalPixelFormat);
  if (!data)
    throw Exception(Exception::kRGSSError, SDL_GetError());

  const std::vector<std::uint8_t> pixels =
      ReadTextureRegion(texture_, 0, 0, size_.x, size_.y);

  auto* target = static_cast<std::uint8_t*>(data->pixels);
  const std::size_t pixel_pitch = static_cast<std::size_t>(size_.x) * 4;
  for (int32_t y = 0; y < size_.y; ++y) {
    std::uint8_t* row = target + static_cast<std::size_t>(y) * data->pitch;
    std::memcpy(row, pixels.data() + static_cast<std::size_t>(y) * pixel_pitch,
                pixel_pitch);
    UnpremultiplyPixelRow(row, size_.x);
  }

  return MakeRefCounted<Palette>(data);
}

void Bitmap::UpdateWithPalette(RefPtr<Palette> palette) {
  Disposable::Guard();

  auto* data = palette->image();
  if (data->w != size_.x || data->h != size_.y)
    throw Exception(Exception::kRGSSError,
                    "palette data size mismatch bitmap size.");

  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(data->pitch) *
                                   data->h);
  SDL_PremultiplyAlpha(data->w, data->h, data->format, data->pixels,
                       data->pitch, data->format, pixels.data(), data->pitch,
                       false);

  wgpu::TexelCopyTextureInfo destination;
  destination.texture = texture_;

  wgpu::TexelCopyBufferLayout buffer_layout;
  buffer_layout.bytesPerRow = data->pitch;
  buffer_layout.rowsPerImage = data->h;

  wgpu::Extent3D target_size;
  target_size.width = data->w;
  target_size.height = data->h;

  g_queue.WriteTexture(&destination, pixels.data(), data->pitch * data->h,
                       &buffer_layout, &target_size);
}

ATTR_DEF(Bitmap, RefPtr<Font>, Font) {
  if (value) {
    font_ = MakeRefCounted<Font>(*value);
    return std::nullopt;
  } else {
    return font_;
  }
}

wgpu::RenderPassEncoder Bitmap::BeginRendering(wgpu::CommandEncoder encoder,
                                               std::optional<glm::vec4> clear) {
  wgpu::RenderPassColorAttachment color_attachment = {
      .view = texture_view_,
      .loadOp = wgpu::LoadOp::Load,
      .storeOp = wgpu::StoreOp::Store,
  };

  if (clear) {
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.clearValue = wgpu::Color{
        .r = clear->r,
        .g = clear->g,
        .b = clear->b,
        .a = clear->a,
    };
  }

  wgpu::RenderPassDepthStencilAttachment depth_stencil_attachment = {
      .view = depth_stencil_view_,
      .depthLoadOp = wgpu::LoadOp::Clear,
      .depthStoreOp = wgpu::StoreOp::Discard,
      .depthClearValue = 1.0f,
      .stencilLoadOp = wgpu::LoadOp::Clear,
      .stencilStoreOp = wgpu::StoreOp::Discard,
      .stencilClearValue = 0,
  };
  wgpu::RenderPassDescriptor pass_desc = {
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
      .depthStencilAttachment = &depth_stencil_attachment,
  };
  return encoder.BeginRenderPass(&pass_desc);
}  // namespace urge

void Bitmap::DisposeObject() {}

void Bitmap::CreateInternal(SDL_Surface* data) {
  size_ = glm::ivec2{data->w, data->h};

  SDL_PremultiplySurfaceAlpha(data, false);

  wgpu::TextureDescriptor texture_desc;
  texture_desc.usage = wgpu::TextureUsage::RenderAttachment |
                       wgpu::TextureUsage::TextureBinding |
                       wgpu::TextureUsage::CopySrc |
                       wgpu::TextureUsage::CopyDst;
  texture_desc.dimension = wgpu::TextureDimension::e2D;
  texture_desc.size.width = data->w;
  texture_desc.size.height = data->h;
  texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
  texture_ = g_device.CreateTexture(&texture_desc);
  texture_view_ = texture_.CreateView(nullptr);

  wgpu::TextureDescriptor depth_stencil_desc;
  depth_stencil_desc.usage = wgpu::TextureUsage::RenderAttachment;
  depth_stencil_desc.dimension = wgpu::TextureDimension::e2D;
  depth_stencil_desc.size.width = data->w;
  depth_stencil_desc.size.height = data->h;
  depth_stencil_desc.format = wgpu::TextureFormat::Depth24PlusStencil8;
  depth_stencil_ = g_device.CreateTexture(&depth_stencil_desc);
  depth_stencil_view_ = depth_stencil_.CreateView(nullptr);

  wgpu::TexelCopyTextureInfo destination;
  destination.texture = texture_;

  wgpu::TexelCopyBufferLayout buffer_layout;
  buffer_layout.bytesPerRow = data->pitch;
  buffer_layout.rowsPerImage = data->h;

  wgpu::Extent3D target_size;
  target_size.width = data->w;
  target_size.height = data->h;

  g_queue.WriteTexture(&destination, data->pixels, data->pitch * data->h,
                       &buffer_layout, &target_size);

  SceneData scene_uniform = {};

  scene_uniform.view_proj_mat = glm::ortho(0.0f, static_cast<float>(size_.x),
                                           static_cast<float>(size_.y), 0.0f);
  ObjectData object_uniform = {};
  object_uniform.model_mat = glm::mat4x4(1.0f);

  wgpu::BufferDescriptor uniform_desc;
  uniform_desc.mappedAtCreation = true;
  uniform_desc.usage = wgpu::BufferUsage::Uniform;
  uniform_desc.size = sizeof(scene_uniform);
  scene_uniform_ = g_device.CreateBuffer(&uniform_desc);
  uniform_desc.size = sizeof(object_uniform);
  object_uniform_ = g_device.CreateBuffer(&uniform_desc);

  std::memcpy(scene_uniform_.GetMappedRange(0, WGPU_WHOLE_MAP_SIZE),
              &scene_uniform, sizeof(scene_uniform));
  scene_uniform_.Unmap();
  std::memcpy(object_uniform_.GetMappedRange(0, WGPU_WHOLE_MAP_SIZE),
              &object_uniform, sizeof(object_uniform));
  object_uniform_.Unmap();

  wgpu::SamplerDescriptor sampler_desc;
  sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
  sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
  sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
  sampler_desc.magFilter = wgpu::FilterMode::Nearest;
  sampler_desc.minFilter = wgpu::FilterMode::Nearest;
  sampler_ = g_device.CreateSampler(&sampler_desc);

  SDL_DestroySurface(data);

  CreateGroup();
}

void Bitmap::CreateGroup() {
  auto pipeline = ShaderSet::Get().state.bitmap.texture_pma;

  scene_group_ = util::CreateBindGroup(pipeline.GetBindGroupLayout(0),
                                       {{0, util::BufferSet(scene_uniform_)}});
  object_group_ = util::CreateBindGroup(
      pipeline.GetBindGroupLayout(1), {{0, util::BufferSet(object_uniform_)}});
  texture_group_ = util::CreateBindGroup(
      pipeline.GetBindGroupLayout(2), {{0, util::TextureViewSet(texture_view_)},
                                       {1, util::SamplerSet(sampler_)}});
}

}  // namespace urge
