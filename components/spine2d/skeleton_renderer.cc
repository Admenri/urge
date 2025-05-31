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

struct SpineTextureAgent {
  RRefPtr<Diligent::ITexture> texture;
};

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
      return premultiplied_alpha ? renderer::NORMAL_PMA : renderer::NORMAL;
    case spine::BlendMode_Additive:
      return premultiplied_alpha ? renderer::ADDITION_PMA : renderer::ADDITION;
    case spine::BlendMode_Multiply:
      return renderer::MULTIPLY;
    case spine::BlendMode_Screen:
      return renderer::SCREEN;
  }
}

void GPUCreateTextureInternal(renderer::RenderDevice* device,
                              SDL_Surface* atlas_surface,
                              SpineTextureAgent* agent,
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
  renderer::CreateTexture2D(**device, &texture, "spine.atlas", atlas_surface,
                            Diligent::USAGE_DEFAULT,
                            Diligent::BIND_SHADER_RESOURCE);

  Diligent::SamplerDesc sampler_desc;
  sampler_desc.Name = "spine2d.atlas.sampler";
  sampler_desc.MinFilter = TexFilterWrap(min_filter);
  sampler_desc.MagFilter = TexFilterWrap(mag_filter);
  sampler_desc.AddressU = TexAddressWrap(uwrap);
  sampler_desc.AddressV = TexAddressWrap(vwrap);

  RRefPtr<Diligent::ISampler> sampler;
  (*device)->CreateSampler(sampler_desc, &sampler);

  auto* atlas_view =
      texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  atlas_view->SetSampler(sampler);

  SDL_DestroySurface(atlas_surface);

  agent->texture = texture;
}

void GPUDestroyTextureInternal(SpineTextureAgent* agent) {
  delete agent;
}

void GPUCreateRendererDataInternal(renderer::RenderDevice* device,
                                   SpineRendererAgent* agent) {
  agent->vertex_batch = SpineVertexBatch::Make(**device);
  agent->shader_binding = device->GetPipelines()->spine2d.CreateBinding();
}

void GPUDestroyRendererDataInternal(SpineRendererAgent* agent) {
  delete agent;
}

void GPUUploadVerticesDataInternal(
    renderer::RenderContext* context,
    SpineRendererAgent* agent,
    base::Vector<renderer::SpineVertex> vertices_data) {
  agent->vertex_batch->QueueWrite(**context, vertices_data.data(),
                                  vertices_data.size());
}

void GPURenderSkeletonCommandsInternal(renderer::RenderDevice* device,
                                       renderer::RenderContext* context,
                                       SpineRendererAgent* agent,
                                       SpineTextureAgent* atlas,
                                       BlendMode blend_mode,
                                       bool premultiplied_alpha,
                                       Diligent::IBuffer** world_buffer,
                                       uint32_t num_vertices,
                                       uint32_t vertices_offset) {
  auto& pipeline_set = device->GetPipelines()->spine2d;
  auto* pipeline = pipeline_set.GetPipeline(
      RenderBlendTypeWrap(blend_mode, premultiplied_alpha));

  // Setup uniform params
  agent->shader_binding->u_transform->Set(*world_buffer);
  agent->shader_binding->u_texture->Set(
      atlas->texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply pipeline state
  (*context)->SetPipelineState(pipeline);
  (*context)->CommitShaderResources(
      **agent->shader_binding,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = **agent->vertex_batch;
  (*context)->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumVertices = num_vertices;
  draw_indexed_attribs.StartVertexLocation = vertices_offset;
  (*context)->Draw(draw_indexed_attribs);
}

}  // namespace

SpineExtension* getDefaultExtension() {
  return new DefaultSpineExtension();
}

DiligentTextureLoader::DiligentTextureLoader(renderer::RenderDevice* device,
                                             base::ThreadWorker* worker,
                                             filesystem::IOService* io_service)
    : device_(device), worker_(worker), io_service_(io_service) {}

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

  auto* agent = new SpineTextureAgent;
  page.width = atlas_surface->w;
  page.height = atlas_surface->h;
  page.texture = agent;

  base::ThreadWorker::PostTask(
      worker_,
      base::BindOnce(&GPUCreateTextureInternal, device_, atlas_surface, agent,
                     page.minFilter, page.magFilter, page.uWrap, page.vWrap));
}

void DiligentTextureLoader::unload(void* texture) {
  base::ThreadWorker::PostTask(
      worker_, base::BindOnce(&GPUDestroyTextureInternal,
                              static_cast<SpineTextureAgent*>(texture)));
}

DiligentRenderer::DiligentRenderer(renderer::RenderDevice* device,
                                   base::ThreadWorker* worker)
    : device_(device),
      worker_(worker),
      agent_(new SpineRendererAgent),
      skeleton_renderer_(base::MakeOwnedPtr<SkeletonRenderer>()),
      pending_commands_(nullptr) {
  base::ThreadWorker::PostTask(
      worker_, base::BindOnce(&GPUCreateRendererDataInternal, device, agent_));
}

DiligentRenderer::~DiligentRenderer() {
  base::ThreadWorker::PostTask(
      worker_, base::BindOnce(&GPUDestroyRendererDataInternal, agent_));
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
  base::ThreadWorker::PostTask(
      worker_, base::BindOnce(&GPUUploadVerticesDataInternal, context, agent_,
                              vertex_cache_));
}

void DiligentRenderer::Render(renderer::RenderContext* context,
                              Diligent::IBuffer** world_buffer,
                              bool premultiplied_alpha) {
  uint32_t vertices_offset = 0;
  RenderCommand* command = pending_commands_;
  while (command) {
    auto* texture = static_cast<SpineTextureAgent*>(command->texture);

    // Raise render task
    base::ThreadWorker::PostTask(
        worker_,
        base::BindOnce(&GPURenderSkeletonCommandsInternal, device_, context,
                       agent_, texture, command->blendMode, premultiplied_alpha,
                       world_buffer, command->numIndices, vertices_offset));

    // Step into next command
    vertices_offset += command->numIndices;
    command = command->next;
  }
}

}  // namespace spine
