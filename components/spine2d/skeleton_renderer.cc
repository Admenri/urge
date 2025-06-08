// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spine2d/skeleton_renderer.h"

#include "SDL3_image/SDL_image.h"
#include "spine/Debug.h"

#include "base/debug/logging.h"
#include "renderer/utils/texture_utils.h"

namespace spine {

namespace {

Diligent::TEXTURE_ADDRESS_MODE TexAddressWrap(TextureWrap wrap) {
  switch (wrap) {
    default:
    case TextureWrap_MirroredRepeat:
      return Diligent::TEXTURE_ADDRESS_MIRROR;
    case TextureWrap_ClampToEdge:
      return Diligent::TEXTURE_ADDRESS_CLAMP;
    case TextureWrap_Repeat:
      return Diligent::TEXTURE_ADDRESS_WRAP;
  }
}

Diligent::FILTER_TYPE TexFilterWrap(TEXTURE_FILTER_ENUM wrap) {
  switch (wrap) {
    default:
    case TextureFilter_Unknown:
      return Diligent::FILTER_TYPE_UNKNOWN;
    case TextureFilter_Nearest:
      return Diligent::FILTER_TYPE_POINT;
    case TextureFilter_Linear:
      return Diligent::FILTER_TYPE_LINEAR;
  }
}

renderer::BlendType RenderBlendTypeWrap(BlendMode mode,
                                        bool premultiplied_alpha) {
  switch (mode) {
    default:
    case spine::BlendMode_Normal:
      return premultiplied_alpha ? renderer::BLEND_TYPE_NORMAL_PMA
                                 : renderer::BLEND_TYPE_NORMAL;
    case spine::BlendMode_Additive:
      return premultiplied_alpha ? renderer::BLEND_TYPE_ADDITION_PMA
                                 : renderer::BLEND_TYPE_ADDITION;
    case spine::BlendMode_Multiply:
      return renderer::BLEND_TYPE_MULTIPLY;
    case spine::BlendMode_Screen:
      return renderer::BLEND_TYPE_SCREEN;
  }
}

}  // namespace

SpineExtension* getDefaultExtension() {
  return new DefaultSpineExtension();
}

DiligentTextureLoader::DiligentTextureLoader(renderer::RenderDevice* device,
                                             filesystem::IOService* io_service)
    : device_(device), io_service_(io_service) {}

DiligentTextureLoader::~DiligentTextureLoader() = default;

void DiligentTextureLoader::load(AtlasPage& page, const String& path) {
  filesystem::IOState io_state;
  SDL_Surface* atlas_surface = nullptr;

  io_service_->OpenRead(
      path.buffer(),
      base::BindRepeating(
          [](SDL_Surface** surf, SDL_IOStream* io, const base::String& ext) {
            *surf = IMG_LoadTyped_IO(io, true, ext.c_str());
            return !!*surf;
          },
          &atlas_surface),
      &io_state);

  if (io_state.error_count) {
    LOG(INFO) << "[Spine2D] " << io_state.error_message;
    return;
  }

  if (!atlas_surface) {
    LOG(INFO) << "[Spine2D] " << SDL_GetError();
    return;
  }

  page.width = atlas_surface->w;
  page.height = atlas_surface->h;
  page.texture = GPUCreateTextureInternal(
      atlas_surface, page.minFilter, page.magFilter, page.uWrap, page.vWrap);
}

void DiligentTextureLoader::unload(void* texture) {
  RRefPtr<Diligent::ITexture> auto_ref;
  auto_ref.Attach(static_cast<Diligent::ITexture*>(texture));
}

void* DiligentTextureLoader::GPUCreateTextureInternal(
    SDL_Surface* atlas_surface,
    TEXTURE_FILTER_ENUM min_filter,
    TEXTURE_FILTER_ENUM mag_filter,
    TextureWrap uwrap,
    TextureWrap vwrap) {
  if (atlas_surface->format != SDL_PIXELFORMAT_ABGR8888) {
    SDL_Surface* conv =
        SDL_ConvertSurface(atlas_surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(atlas_surface);
    atlas_surface = conv;
  }

  RRefPtr<Diligent::ITexture> texture;
  renderer::CreateTexture2D(**device_, &texture, "spine.atlas", atlas_surface,
                            Diligent::USAGE_DEFAULT,
                            Diligent::BIND_SHADER_RESOURCE);

  Diligent::SamplerDesc sampler_desc;
  sampler_desc.Name = "spine2d.atlas.sampler";
  sampler_desc.MinFilter = TexFilterWrap(min_filter);
  sampler_desc.MagFilter = TexFilterWrap(mag_filter);
  sampler_desc.AddressU = TexAddressWrap(uwrap);
  sampler_desc.AddressV = TexAddressWrap(vwrap);

  RRefPtr<Diligent::ISampler> sampler;
  (*device_)->CreateSampler(sampler_desc, &sampler);

  auto* atlas_view =
      texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  atlas_view->SetSampler(sampler);

  SDL_DestroySurface(atlas_surface);

  return texture.Detach();
}

DiligentRenderer::DiligentRenderer(renderer::RenderDevice* device)
    : device_(device),
      vertex_batch_(SpineVertexBatch::Make(**device)),
      shader_binding_(device->GetPipelines()->spine2d.CreateBinding()),
      skeleton_renderer_(base::MakeOwnedPtr<SkeletonRenderer>()),
      pending_commands_(nullptr) {}

DiligentRenderer::~DiligentRenderer() {
  vertex_batch_ = SpineVertexBatch();
  shader_binding_ = renderer::Binding_Base();
}

void DiligentRenderer::Update(renderer::RenderContext* context,
                              spine::Skeleton* skeleton) {
  vertex_cache_.clear();
  pending_commands_ = skeleton_renderer_->render(*skeleton);
  RenderCommand* command = pending_commands_;
  while (command) {
    float* positions = command->positions;
    float* uvs = command->uvs;
    uint32_t* colors = command->colors;
    uint32_t* darkColors = command->darkColors;

    // Vertex data
    for (int32_t i = 0; i < command->numIndices; ++i) {
      uint16_t index = command->indices[i];
      uint16_t vertex_index = index << 1;

      renderer::SpineVertex vertex;
      vertex.position.x = positions[vertex_index];
      vertex.position.y = positions[vertex_index + 1];
      vertex.texcoord.x = uvs[vertex_index];
      vertex.texcoord.y = uvs[vertex_index + 1];
      uint32_t color = colors[vertex_index >> 1];
      vertex.light_color = (color & 0xFF00FF00) | ((color & 0x00FF0000) >> 16) |
                           ((color & 0x000000FF) << 16);
      uint32_t darkColor = darkColors[vertex_index >> 1];
      vertex.dark_color = (darkColor & 0xFF00FF00) |
                          ((darkColor & 0x00FF0000) >> 16) |
                          ((darkColor & 0x000000FF) << 16);
      vertex_cache_.push_back(vertex);
    }

    command = command->next;
  }

  // Upload data
  vertex_batch_.QueueWrite(**context, vertex_cache_.data(),
                           vertex_cache_.size());
}

void DiligentRenderer::Render(renderer::RenderContext* context,
                              Diligent::IBuffer* world_buffer,
                              bool premultiplied_alpha) {
  uint32_t vertices_offset = 0;
  RenderCommand* command = pending_commands_;
  while (command) {
    auto* texture = static_cast<Diligent::ITexture*>(command->texture);

    // Raise render task
    GPURenderSkeletonCommandsInternal(context, texture, command->blendMode,
                                      premultiplied_alpha, world_buffer,
                                      command->numIndices, vertices_offset);

    // Step into next command
    vertices_offset += command->numIndices;
    command = command->next;
  }
}

void DiligentRenderer::GPURenderSkeletonCommandsInternal(
    renderer::RenderContext* render_context,
    Diligent::ITexture* atlas,
    BlendMode blend_mode,
    bool premultiplied_alpha,
    Diligent::IBuffer* world_buffer,
    uint32_t num_vertices,
    uint32_t vertices_offset) {
  auto& pipeline_set = device_->GetPipelines()->spine2d;
  auto* pipeline = pipeline_set.GetPipeline(
      RenderBlendTypeWrap(blend_mode, premultiplied_alpha));

  // Setup uniform params
  shader_binding_.u_transform->Set(world_buffer);
  shader_binding_.u_texture->Set(
      atlas->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply pipeline state
  (*render_context)->SetPipelineState(pipeline);
  (*render_context)
      ->CommitShaderResources(
          *shader_binding_,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *vertex_batch_;
  (*render_context)
      ->SetVertexBuffers(0, 1, &vertex_buffer, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumVertices = num_vertices;
  draw_indexed_attribs.StartVertexLocation = vertices_offset;
  (*render_context)->Draw(draw_indexed_attribs);
}

}  // namespace spine
