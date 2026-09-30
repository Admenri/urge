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
#include "core/pipeline.h"

namespace urge {

static const SDL_PixelFormat kInternalPixelFormat = SDL_PIXELFORMAT_ABGR8888;

namespace {

//! Converts an RGSS color (components in [0, 255]) into the normalized and
//! premultiplied vertex color the engine shaders expect: the blend state of the
//! engine is BlendType::kNormal (One / InvSrcAlpha) and bitmap contents are
//! stored with premultiplied alpha, so the RGB components have to be multiplied
//! with the alpha channel.
glm::vec4 PremultiplyColor(const glm::vec4& color) {
  glm::vec4 result = color / 255.0f;
  result.r *= result.a;
  result.g *= result.a;
  result.b *= result.a;
  return result;
}

//! Converts a stored (premultiplied) pixel into an RGSS color, i.e. divides the
//! RGB components by the alpha channel again.
glm::vec4 UnpremultiplyColor(const std::uint8_t pixel[4]) {
  const float alpha = static_cast<float>(pixel[3]);
  if (alpha <= 0.0f)
    return glm::vec4(0.0f);

  return glm::vec4(
      std::min(255.0f, static_cast<float>(pixel[0]) * 255.0f / alpha),
      std::min(255.0f, static_cast<float>(pixel[1]) * 255.0f / alpha),
      std::min(255.0f, static_cast<float>(pixel[2]) * 255.0f / alpha), alpha);
}

//! Converts an RGSS opacity (in [0, 255]) into a premultiplied white vertex
//! color, which scales the sampled color of a blit.
glm::vec4 PremultiplyOpacity(int32_t opacity) {
  const float value =
      std::clamp(static_cast<float>(opacity) / 255.0f, 0.0f, 1.0f);
  return glm::vec4(value, value, value, value);
}

//! Normalized texture coordinates of a source rectangle inside a bitmap.
RectF NormalizeTexcoord(const RectI& src_rect, const glm::ivec2& src_size) {
  const float width = static_cast<float>(src_size.x);
  const float height = static_cast<float>(src_size.y);

  if (width <= 0.0f || height <= 0.0f)
    return RectF();

  return RectF(static_cast<float>(src_rect.x) / width,
               static_cast<float>(src_rect.y) / height,
               static_cast<float>(src_rect.width) / width,
               static_cast<float>(src_rect.height) / height);
}

//! Converts one row of premultiplied pixels into straight ones, in place, i.e.
//! the inverse of the SDL_PremultiplyAlpha() conversion used the other way
//! around: the color channels are divided by the alpha channel again.
//! \remarks The internal pixel format is SDL_PIXELFORMAT_ABGR8888, so the bytes
//! of one pixel are red, green, blue and alpha.
void UnpremultiplyPixelRow(std::uint8_t* pixels, int32_t width) {
  for (int32_t x = 0; x < width; ++x, pixels += 4) {
    const std::uint32_t alpha = pixels[3];

    if (alpha == 0) {
      // A fully transparent pixel has no color information left
      pixels[0] = 0;
      pixels[1] = 0;
      pixels[2] = 0;
    } else if (alpha != 0xFF) {
      for (std::size_t channel = 0; channel < 3; ++channel) {
        // Rounded division, an additive blend can raise the color above alpha
        const std::uint32_t value =
            (static_cast<std::uint32_t>(pixels[channel]) * 255 + alpha / 2) /
            alpha;
        pixels[channel] = static_cast<std::uint8_t>(std::min(255u, value));
      }
    }
  }
}

//! Outcome of an asynchronous buffer mapping, filled in by the callback the
//! device invokes once the copy has been submitted.
struct MapResult {
  //! The callback runs but once, so the state below is the final one.
  bool invoked = false;
  WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
  std::string error;
};

//! A readable form of the status of a mapping, the callback of a valid mapping
//! is not always given a reason.
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

//! Reads back the width x height texels of texture which start at (x, y) into
//! host memory. A texture itself can not be mapped, so the region is copied
//! into a mappable staging buffer first, which is then mapped and waited for.
//! The returned pixels are packed one after another, four bytes per pixel.
//! \remarks The textures of this class use RGBA8Unorm, so the bytes of a pixel
//! are red, green, blue and alpha, and they store premultiplied alpha.
std::vector<std::uint8_t> ReadTextureRegion(wgpu::Texture texture,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height) {
  if (width <= 0 || height <= 0)
    return {};

  auto& gpu = GPUDevice::Get();

  /* The row pitch of a texture to buffer copy is aligned to the 256 bytes block
     size of the device */
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

  // The copy is asynchronous, so the mapping is waited for before reading
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

  staging.MapAsync(wgpu::MapMode::Read, 0, byte_size, map_callback);

  /* GPUDevice::WaitAny cannot be used here: wgpu-native does not implement the
     future API yet, its wgpuBufferMapAsync() ends with a "TODO: Properly handle
     futures" and returns an empty future, so there is nothing to wait for and
     the callback above would never run. Polling the device blocks on the queue
     and is what completes the mapping. */
  gpu.Poll(true);

  /* GetMappedRange() is what raises the "buffer is not mapped" validation
     error, and wgpu-native panics on it instead of reporting it, so the status
     of the mapping has to be checked before the pointer is asked for. */
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

  // The aligned row pitch of the staging buffer is dropped here
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

Bitmap::Bitmap(std::string filename) {
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

Bitmap::Bitmap(int32_t width, int32_t height) {
  SDL_Surface* image = SDL_CreateSurface(width, height, kInternalPixelFormat);

  CreateInternal(image);
}

Bitmap::Bitmap(RefPtr<Bitmap> other)
    : Bitmap(other->size().x, other->size().y) {
  auto encoder = GPUDevice::Get().device().CreateCommandEncoder(nullptr);

  wgpu::TexelCopyTextureInfo source, destination;
  source.texture = other->texture();
  destination.texture = texture();

  wgpu::Extent3D copy_size;
  copy_size.width = other->size().x;
  copy_size.height = other->size().y;

  encoder.CopyTextureToTexture(&source, &destination, &copy_size);

  auto command = encoder.Finish(nullptr);
  GPUDevice::Get().queue().Submit(1, &command);
}

Bitmap::~Bitmap() {
  Disposable::Dispose();
}

int32_t Bitmap::GetWidth() {
  return size_.x;
}

int32_t Bitmap::GetHeight() {
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

  auto vertices_data =
      primitive_
          .EmitQuad(dst_rect->data, MakeNorm(src_rect->data, src_bitmap->size_),
                    glm::vec4(opacity / 255.0f))
          .End();

  auto buffer = AcquireBuffer(vertices_data.size_bytes());
  GPUDevice::Get().queue().WriteBuffer(buffer, 0, vertices_data.data(),
                                       vertices_data.size_bytes());

  auto encoder = GPUDevice::Get().device().CreateCommandEncoder(nullptr);
  wgpu::RenderPassColorAttachment color_attachment;
  color_attachment.view = view_;
  color_attachment.loadOp = wgpu::LoadOp::Load;
  color_attachment.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor pass_desc;
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &color_attachment;
  auto pass = encoder.BeginRenderPass(&pass_desc);
  {
    auto pipeline = ShaderSet::Get().state.texture_pma;
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, scene_group_, 0, nullptr);
    pass.SetBindGroup(1, object_group_, 0, nullptr);
    pass.SetBindGroup(2, src_bitmap->texture_group_, 0, nullptr);
    pass.SetVertexBuffer(0, buffer, 0, WGPU_WHOLE_SIZE);
    pass.Draw(6, 1, 0, 0);
  }
  pass.End();
  auto command = encoder.Finish(nullptr);
  GPUDevice::Get().queue().Submit(1, &command);
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

  auto color1_norm = PremultiplyColor(color1->Normalize());
  auto color2_norm = PremultiplyColor(color2->Normalize());

  if (vertical)
    primitive_.EmitQuad(RectI(x, y, width, height), RectF(), color1_norm,
                        color1_norm, color2_norm, color2_norm);
  else
    primitive_.EmitQuad(RectI(x, y, width, height), RectF(), color1_norm,
                        color2_norm, color1_norm, color2_norm);
  auto vertices_data = primitive_.End();

  auto buffer = AcquireBuffer(vertices_data.size_bytes());
  GPUDevice::Get().queue().WriteBuffer(buffer, 0, vertices_data.data(),
                                       vertices_data.size_bytes());

  auto encoder = GPUDevice::Get().device().CreateCommandEncoder(nullptr);
  wgpu::RenderPassColorAttachment color_attachment;
  color_attachment.view = view_;
  color_attachment.loadOp = wgpu::LoadOp::Load;
  color_attachment.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor pass_desc;
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &color_attachment;
  auto pass = encoder.BeginRenderPass(&pass_desc);
  {
    auto pipeline = ShaderSet::Get().state.color_noblend;
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, scene_group_, 0, nullptr);
    pass.SetBindGroup(1, object_group_, 0, nullptr);
    pass.SetVertexBuffer(0, buffer, 0, WGPU_WHOLE_SIZE);
    pass.Draw(6, 1, 0, 0);
  }
  pass.End();
  auto command = encoder.Finish(nullptr);
  GPUDevice::Get().queue().Submit(1, &command);
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

  // Transparent black, the same color an untouched bitmap has
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

  // Coordinates outside of the bitmap read as transparent black, the color an
  // untouched bitmap has
  if (x < 0 || y < 0 || x >= size_.x || y >= size_.y)
    return MakeRefCounted<Color>();

  const std::vector<std::uint8_t> pixels =
      ReadTextureRegion(texture_, x, y, 1, 1);

  // The texture stores premultiplied alpha while RGSS colors are straight ones
  const glm::vec4 color = UnpremultiplyColor(pixels.data());
  return MakeRefCounted<Color>(color.r, color.g, color.b, color.a);
}

void Bitmap::SetPixel(int32_t x, int32_t y, RefPtr<Color> color) {
  GradientFillRect(x, y, 1, 1, color, color);
}

void Bitmap::HueChange(int32_t hue) {
  Disposable::Guard();

  // TODO
}

void Bitmap::Blur() {
  Disposable::Guard();

  // TODO
}

void Bitmap::RadialBlur(int32_t angle, int32_t division) {
  Disposable::Guard();

  // TODO
}

void Bitmap::DrawText(int32_t x,
                      int32_t y,
                      int32_t width,
                      int32_t height,
                      std::string str,
                      int32_t align) {
  Disposable::Guard();

  // TODO
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

  // TODO
  return MakeRefCounted<Rect>();
}

RefPtr<Palette> Bitmap::ToPalette() {
  Disposable::Guard();

  auto* data = SDL_CreateSurface(size_.x, size_.y, kInternalPixelFormat);
  if (!data)
    throw Exception(Exception::kRGSSError, SDL_GetError());

  // The whole texture is read back into host memory
  const std::vector<std::uint8_t> pixels =
      ReadTextureRegion(texture_, 0, 0, size_.x, size_.y);

  /* The texture stores premultiplied alpha while a palette holds straight
     colors, so the read back pixels are converted row by row */
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

  /* A palette holds straight colors while the textures of the engine store
     premultiplied ones. The pixels are converted into a scratch buffer, so the
     palette of the caller is left untouched */
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(data->pitch) *
                                   data->h);
  SDL_PremultiplyAlpha(data->w, data->h, data->format, data->pixels,
                       data->pitch, data->format, pixels.data(), data->pitch,
                       false);

  // Update texture data
  wgpu::TexelCopyTextureInfo destination;
  destination.texture = texture_;

  wgpu::TexelCopyBufferLayout buffer_layout;
  buffer_layout.bytesPerRow = data->pitch;
  buffer_layout.rowsPerImage = data->h;

  wgpu::Extent3D target_size;
  target_size.width = data->w;
  target_size.height = data->h;

  GPUDevice::Get().queue().WriteTexture(&destination, pixels.data(),
                                        data->pitch * data->h, &buffer_layout,
                                        &target_size);
}

ATTR_DEF(Bitmap, RefPtr<Font>, Font) {
  if (value) {
    font_ = *value;
    return std::nullopt;
  } else {
    return font_;
  }
}

void Bitmap::DisposeObject() {}

void Bitmap::CreateInternal(SDL_Surface* data) {
  size_ = glm::ivec2{data->w, data->h};

  // The blend state of the engine and the drawing operations of this class
  // store premultiplied alpha, so the pixel data is converted here as well
  SDL_PremultiplySurfaceAlpha(data, false);

  // Color data
  wgpu::TextureDescriptor texture_desc;
  texture_desc.usage = wgpu::TextureUsage::RenderAttachment |
                       wgpu::TextureUsage::TextureBinding |
                       wgpu::TextureUsage::CopySrc |
                       wgpu::TextureUsage::CopyDst;
  texture_desc.dimension = wgpu::TextureDimension::e2D;
  texture_desc.size.width = data->w;
  texture_desc.size.height = data->h;
  texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
  texture_ = GPUDevice::Get().device().CreateTexture(&texture_desc);
  view_ = texture_.CreateView(nullptr);

  // Depth stencil
  wgpu::TextureDescriptor depth_stencil_desc;
  depth_stencil_desc.usage = wgpu::TextureUsage::RenderAttachment;
  depth_stencil_desc.dimension = wgpu::TextureDimension::e2D;
  depth_stencil_desc.size.width = data->w;
  depth_stencil_desc.size.height = data->h;
  depth_stencil_desc.format = wgpu::TextureFormat::Depth24PlusStencil8;
  depth_stencil_ = GPUDevice::Get().device().CreateTexture(&depth_stencil_desc);

  // Update texture data
  wgpu::TexelCopyTextureInfo destination;
  destination.texture = texture_;

  wgpu::TexelCopyBufferLayout buffer_layout;
  buffer_layout.bytesPerRow = data->pitch;
  buffer_layout.rowsPerImage = data->h;

  wgpu::Extent3D target_size;
  target_size.width = data->w;
  target_size.height = data->h;

  GPUDevice::Get().queue().WriteTexture(&destination, data->pixels,
                                        data->pitch * data->h, &buffer_layout,
                                        &target_size);

  // Uniform data
  SceneData scene_uniform = {};
  /* The arguments are spelled as floats on purpose: glm::ortho is a template,
     an integer argument list would deduce T = int and the divisions inside
     the function would be carried out in integer arithmetic, which turns the
     scales of the matrix into zero and collapses every vertex. */
  scene_uniform.view_proj_mat = glm::ortho(0.0f, static_cast<float>(size_.x),
                                           static_cast<float>(size_.y), 0.0f);
  ObjectData object_uniform = {};
  object_uniform.model_mat = glm::mat4x4(1.0f);

  wgpu::BufferDescriptor uniform_desc;
  uniform_desc.mappedAtCreation = true;
  uniform_desc.usage = wgpu::BufferUsage::Uniform;
  uniform_desc.size = sizeof(scene_uniform);
  scene_uniform_ = GPUDevice::Get().device().CreateBuffer(&uniform_desc);
  uniform_desc.size = sizeof(object_uniform);
  object_uniform_ = GPUDevice::Get().device().CreateBuffer(&uniform_desc);

  std::memcpy(scene_uniform_.GetMappedRange(0, WGPU_WHOLE_MAP_SIZE),
              &scene_uniform, sizeof(scene_uniform));
  scene_uniform_.Unmap();
  std::memcpy(object_uniform_.GetMappedRange(0, WGPU_WHOLE_MAP_SIZE),
              &object_uniform, sizeof(object_uniform));
  object_uniform_.Unmap();

  // Sampler
  wgpu::SamplerDescriptor sampler_desc;
  sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
  sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
  sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
  sampler_desc.magFilter = wgpu::FilterMode::Nearest;
  sampler_desc.minFilter = wgpu::FilterMode::Nearest;
  sampler_ = GPUDevice::Get().device().CreateSampler(&sampler_desc);

  // Release cpu data
  SDL_DestroySurface(data);

  CreateGroup();
}

void Bitmap::CreateGroup() {
  auto pipeline = ShaderSet::Get().state.texture_pma;

  scene_group_ = CreateWGroup(pipeline.GetBindGroupLayout(0),
                              {{0, WBufferSet(scene_uniform_)}});
  object_group_ = CreateWGroup(pipeline.GetBindGroupLayout(1),
                               {{0, WBufferSet(object_uniform_)}});
  texture_group_ =
      CreateWGroup(pipeline.GetBindGroupLayout(2),
                   {{0, WTextureViewSet(view_)}, {1, WSamplerSet(sampler_)}});
}

wgpu::Buffer Bitmap::AcquireBuffer(size_t size) {
  if (!vertex_buffer_ || vertex_buffer_.GetSize() < size) {
    wgpu::BufferDescriptor buffer_desc;
    buffer_desc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    buffer_desc.size = size;
    vertex_buffer_ = GPUDevice::Get().device().CreateBuffer(&buffer_desc);
  }

  return vertex_buffer_;
}

}  // namespace urge
