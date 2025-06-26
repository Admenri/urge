// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/cubismsprite_impl.h"

#include "Id/CubismIdManager.hpp"

#include "renderer/utils/texture_utils.h"

namespace content {

namespace {

struct CubismAllocator : public Csm::ICubismAllocator {
  void* Allocate(const Csm::csmSizeType size) { return mi_malloc(size); }

  void Deallocate(void* memory) { mi_free(memory); }

  void* AllocateAligned(const Csm::csmSizeType size,
                        const Csm::csmUint32 alignment) {
    return mi_aligned_alloc(alignment, size);
  }

  void DeallocateAligned(void* alignedMemory) { mi_free(alignedMemory); }
};

Csm::CubismFramework::Option g_cubism_option;
CubismAllocator g_cubism_allocator;
filesystem::IOService* g_cubism_ioservice = nullptr;

Csm::csmByte* LoadFileAsBytes(const std::string filePath,
                              Csm::csmSizeInt* outSize) {
  filesystem::IOState state;
  SDL_IOStream* file_stream =
      g_cubism_ioservice->OpenReadRaw(filePath.c_str(), &state);
  if (state.error_count) {
    LOG(ERROR) << "[Cubism] " << state.error_message;
    return nullptr;
  }

  int64_t file_size = file_stream ? SDL_GetIOSize(file_stream) : 0;
  Csm::csmByte* buffer = static_cast<Csm::csmByte*>(SDL_malloc(file_size));
  if (buffer)
    SDL_ReadIO(file_stream, buffer, file_size);
  *outSize = static_cast<uint32_t>(file_size);

  if (file_stream)
    SDL_CloseIO(file_stream);

  return buffer;
}

void ReleaseBytes(Csm::csmByte* byteData) {
  if (byteData)
    SDL_free(byteData);
}

}  // namespace

scoped_refptr<CubismSprite> CubismSprite::New(
    ExecutionContext* execution_context,
    const base::String& model_dir,
    const base::String& model_json,
    ExceptionState& exception_state) {
  base::OwnedPtr<cubism_render::CubismModel> model =
      base::MakeOwnedPtr<cubism_render::CubismModel>(
          execution_context->io_service, execution_context->render_device);

  if (!model->LoadAssets(model_dir.c_str(), model_json.c_str())) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to load cubism assets.");
    return nullptr;
  }

  return base::MakeRefCounted<CubismSpriteImpl>(execution_context,
                                                std::move(model));
}

void CubismSpriteImpl::InitializeCubismFramework(
    int32_t buffer_num,
    renderer::RenderDevice* device,
    filesystem::IOService* io_service) {
  g_cubism_ioservice = io_service;

  // Setup cubism
  g_cubism_option.LoadFileFunction = LoadFileAsBytes;
  g_cubism_option.ReleaseBytesFunction = ReleaseBytes;
  g_cubism_option.LoggingLevel = Csm::CubismFramework::Option::LogLevel_Verbose;
  g_cubism_option.LogFunction = [](const char* message) {
    LOG(INFO) << message;
  };

  // Start up
  Csm::CubismFramework::StartUp(&g_cubism_allocator, &g_cubism_option);

  // Initialize cubism
  Csm::CubismFramework::Initialize();

  // Initialize device
  Csm::Rendering::CubismRenderer_Diligent::InitializeConstantSettings(
      buffer_num, device);
}

void CubismSpriteImpl::DestroyCubismFramework() {
  // Release Cubism SDK
  Csm::CubismFramework::Dispose();
}

CubismSpriteImpl::CubismSpriteImpl(
    ExecutionContext* execution_context,
    base::OwnedPtr<cubism_render::CubismModel> model)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      node_(execution_context->screen_drawable_node, SortKey()),
      model_(std::move(model)) {
  node_.RegisterEventHandler(base::BindRepeating(
      &CubismSpriteImpl::DrawableNodeHandlerInternal, base::Unretained(this)));

  base::Vec2i canvas_pixel_size;
  canvas_pixel_size.x = model_->GetModel()->GetCanvasWidthPixel();
  canvas_pixel_size.y = model_->GetModel()->GetCanvasHeightPixel();

  GPUCreateCubismSpriteInternal(canvas_pixel_size);
}

CubismSpriteImpl::~CubismSpriteImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void CubismSpriteImpl::SetLabel(const base::String& label,
                                ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetDebugLabel(label);
}

void CubismSpriteImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool CubismSpriteImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void CubismSpriteImpl::SetDragging(float x,
                                   float y,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK;

  model_->SetDragging(x, y);
}

void CubismSpriteImpl::SetExpression(const base::String& expression_name,
                                     ExceptionState& exception_state) {
  DISPOSE_CHECK;

  model_->SetExpression(expression_name.c_str());
}

void CubismSpriteImpl::SetRandomExpression(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  model_->SetRandomExpression();
}

void CubismSpriteImpl::StartMotion(const base::String& group,
                                   int32_t no,
                                   int32_t priority,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK;

  model_->StartMotion(group.c_str(), no, priority);
}

void CubismSpriteImpl::StartRandomMotion(const base::String& group,
                                         int32_t priority,
                                         ExceptionState& exception_state) {
  DISPOSE_CHECK;

  model_->StartRandomMotion(group.c_str(), priority);
}

bool CubismSpriteImpl::HitTest(const base::String& area_name,
                               float x,
                               float y,
                               ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return model_->HitTest(area_name.c_str(), x, y);
}

float CubismSpriteImpl::GetParameter(const base::String& name,
                                     ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  auto* csm_model = model_->GetModel();
  auto csm_id = Csm::CubismFramework::GetIdManager()->GetId(name.c_str());

  return csm_model->GetParameterValue(csm_id);
}

void CubismSpriteImpl::SetParameter(const base::String& name,
                                    float value,
                                    float weight,
                                    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* csm_model = model_->GetModel();
  auto csm_id = Csm::CubismFramework::GetIdManager()->GetId(name.c_str());

  csm_model->SetParameterValue(csm_id, value, weight);
}

void CubismSpriteImpl::AddParameter(const base::String& name,
                                    float value,
                                    float weight,
                                    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* csm_model = model_->GetModel();
  auto csm_id = Csm::CubismFramework::GetIdManager()->GetId(name.c_str());

  csm_model->AddParameterValue(csm_id, value, weight);
}

scoped_refptr<Viewport> CubismSpriteImpl::Get_Viewport(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return viewport_;
}

void CubismSpriteImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                                    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                   : context()->screen_drawable_node);
}

bool CubismSpriteImpl::Get_Visible(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return node_.GetVisibility();
}

void CubismSpriteImpl::Put_Visible(const bool& value,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeVisibility(value);
}

float CubismSpriteImpl::Get_X(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return position_.x;
}

void CubismSpriteImpl::Put_X(const float& value,
                             ExceptionState& exception_state) {
  DISPOSE_CHECK;

  position_.x = value;
}

float CubismSpriteImpl::Get_Y(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return position_.y;
}

void CubismSpriteImpl::Put_Y(const float& value,
                             ExceptionState& exception_state) {
  DISPOSE_CHECK;

  position_.y = value;
}

int32_t CubismSpriteImpl::Get_Z(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return node_.GetSortKeys()->weight[0];
}

void CubismSpriteImpl::Put_Z(const int32_t& value,
                             ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeSortWeight(value);
}

float CubismSpriteImpl::Get_ZoomX(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return scale_.x;
}

void CubismSpriteImpl::Put_ZoomX(const float& value,
                                 ExceptionState& exception_state) {
  DISPOSE_CHECK;

  scale_.x = value;
}

float CubismSpriteImpl::Get_ZoomY(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return scale_.y;
}

void CubismSpriteImpl::Put_ZoomY(const float& value,
                                 ExceptionState& exception_state) {
  DISPOSE_CHECK;

  scale_.y = value;
}

int32_t CubismSpriteImpl::Get_Opacity(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return opacity_;
}

void CubismSpriteImpl::Put_Opacity(const int32_t& value,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK;

  opacity_ = value;
}

int32_t CubismSpriteImpl::Get_BlendType(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return blend_type_;
}

void CubismSpriteImpl::Put_BlendType(const int32_t& value,
                                     ExceptionState& exception_state) {
  DISPOSE_CHECK;

  blend_type_ = value;
}

void CubismSpriteImpl::OnObjectDisposed() {
  node_.DisposeNode();

  model_.reset();
  viewport_.reset();
}

void CubismSpriteImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (stage == DrawableNode::BEFORE_RENDER) {
    GPURenderCubismModelOffscreenInternal(params->context);
  } else if (stage == DrawableNode::ON_RENDERING) {
    GPURenderCanvasInternal(params->context, params->world_binding);
  }
}

void CubismSpriteImpl::GPUCreateCubismSpriteInternal(
    const base::Vec2i& canvas_size) {
  model_->GetRenderBuffer().CreateOffscreenSurface(
      context()->render_device, canvas_size.x, canvas_size.y);

  agent_.quad = renderer::QuadBatch::Make(**context()->render_device, 1);
  agent_.binding =
      context()->render_device->GetPipelines()->base.CreateBinding();
}

void CubismSpriteImpl::GPURenderCubismModelOffscreenInternal(
    Diligent::IDeviceContext* render_context) {
  // Offscreen rendering cubism model
  base::Vec2i canvas_size;
  canvas_size.x = model_->GetRenderBuffer().GetBufferWidth();
  canvas_size.y = model_->GetRenderBuffer().GetBufferHeight();

  {
    Csm::CubismMatrix44 projection;
    projection.LoadIdentity();

    if (model_->GetModel()->GetCanvasWidth() > 1.0f &&
        canvas_size.x < canvas_size.y) {
      model_->GetModelMatrix()->SetWidth(2.0f);
      projection.Scale(1.0f, static_cast<float>(canvas_size.x) /
                                 static_cast<float>(canvas_size.y));
    } else {
      projection.Scale(
          static_cast<float>(canvas_size.y) / static_cast<float>(canvas_size.x),
          1.0f);
    }

    auto& offscreen_canvas = model_->GetRenderBuffer();
    Csm::Rendering::CubismRenderer_Diligent::StartFrame(render_context,
                                                        &offscreen_canvas);

    offscreen_canvas.BeginDraw(render_context);
    offscreen_canvas.Clear(render_context, 0, 0, 0, 0);

    model_->Update();
    model_->Draw(projection);

    offscreen_canvas.EndDraw(render_context);

    Csm::Rendering::CubismRenderer_Diligent::EndFrame();
  }

  // Prepare display single quad
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(
      &transient_quad,
      base::RectF(position_, base::Vec2(canvas_size) * scale_));
  renderer::Quad::SetTexCoordRectNorm(&transient_quad, base::Rect(0, 1));
  renderer::Quad::SetColor(&transient_quad, base::Vec4(opacity_ / 255.0f));

  agent_.quad.QueueWrite(render_context, &transient_quad);
}

void CubismSpriteImpl::GPURenderCanvasInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::IBuffer* world_buffer) {
  auto& pipeline_set = context()->render_device->GetPipelines()->base;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL, true);

  agent_.binding.u_transform->Set(world_buffer);
  agent_.binding.u_texture->Set(model_->GetRenderBuffer().GetTextureView());

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *agent_.binding, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *agent_.quad;
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **context()->render_device->GetQuadIndex(), 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  render_context->DrawIndexed(draw_indexed_attribs);
}

}  // namespace content
