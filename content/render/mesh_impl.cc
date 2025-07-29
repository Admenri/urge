// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/mesh_impl.h"

#include "content/context/execution_context.h"

namespace content {

static constexpr char kShaderWorldMatrixVariableName[] = "WorldMatrix";

scoped_refptr<Mesh> Mesh::New(ExecutionContext* execution_context,
                              scoped_refptr<Viewport> viewport,
                              ExceptionState& exception_state) {
  return base::MakeRefCounted<MeshImpl>(execution_context,
                                        ViewportImpl::From(viewport));
}

MeshImpl::MeshImpl(ExecutionContext* execution_context,
                   scoped_refptr<ViewportImpl> parent)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      node_(parent ? parent->GetDrawableController()
                   : execution_context->screen_drawable_node,
            SortKey()) {
  node_.RegisterEventHandler(base::BindRepeating(
      &MeshImpl::DrawableNodeHandlerInternal, base::Unretained(this)));
}

DISPOSABLE_DEFINITION(MeshImpl);

void MeshImpl::SetLabel(const std::string& label,
                        ExceptionState& exception_state) {
  node_.SetDebugLabel(label);
}

void MeshImpl::SetVertexBuffers(
    uint32_t start_slot,
    const std::vector<scoped_refptr<GPUBuffer>>& buffers,
    const std::vector<uint64_t>& offsets,
    GPU::SetVertexBuffersFlags flags,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  vertex_buffers_.start_slot = start_slot;
  vertex_buffers_.buffers.clear();
  for (auto& it : buffers) {
    auto* buffer_impl = static_cast<BufferImpl*>(it.get());
    RRefPtr<Diligent::IBuffer> buffer_object(
        buffer_impl ? buffer_impl->AsRawPtr() : nullptr);
    vertex_buffers_.buffers.push_back(std::move(buffer_object));
  }
  vertex_buffers_.offsets = offsets;
  vertex_buffers_.flags =
      static_cast<Diligent::SET_VERTEX_BUFFERS_FLAGS>(flags);
}

void MeshImpl::SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                              uint64_t byte_offset,
                              ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* buffer_object = static_cast<BufferImpl*>(buffer.get());
  index_buffer_.index_buffer =
      buffer_object ? buffer_object->AsRawPtr() : nullptr;
  index_buffer_.byte_offset = byte_offset;
}

void MeshImpl::SetDrawAttribs(uint32_t num_vertices,
                              uint32_t num_instances,
                              uint32_t first_vertex,
                              uint32_t first_instance,
                              ExceptionState& exception_state) {
  DrawAttribs attribs;
  attribs.num_vertices = num_vertices;
  attribs.num_instances = num_instances;
  attribs.first_vertex = first_vertex;
  attribs.first_instance = first_instance;

  draw_attribs_ = std::move(attribs);
}

void MeshImpl::SetDrawAttribs(uint32_t num_indices,
                              uint32_t num_instances,
                              uint32_t first_index,
                              uint32_t base_vertex,
                              uint32_t first_instance,
                              GPU::ValueType index_type,
                              ExceptionState& exception_state) {
  DrawIndexedAttribs attribs;
  attribs.num_indices = num_indices;
  attribs.num_instances = num_instances;
  attribs.first_index = first_index;
  attribs.base_vertex = base_vertex;
  attribs.first_instance = first_instance;
  attribs.index_type = static_cast<Diligent::VALUE_TYPE>(index_type);

  draw_attribs_ = std::move(attribs);
}

void MeshImpl::SetDrawAttribs(
    scoped_refptr<GPUBuffer> attribs_buffer,
    uint64_t draw_args_offset,
    uint32_t draw_count,
    uint32_t draw_args_stride,
    GPU::ResourceStateTransitionMode attribs_buffer_mode,
    scoped_refptr<GPUBuffer> counter_buffer,
    uint64_t counter_offset,
    GPU::ResourceStateTransitionMode counter_buffer_mode,
    ExceptionState& exception_state) {
  auto* attribs_buffer_object = static_cast<BufferImpl*>(attribs_buffer.get());
  auto* counter_buffer_object = static_cast<BufferImpl*>(counter_buffer.get());

  DrawIndirectAttribs attribs;
  attribs.attribs_buffer =
      attribs_buffer_object ? attribs_buffer_object->AsRawPtr() : nullptr;
  attribs.draw_args_offset = draw_args_offset;
  attribs.draw_count = draw_count;
  attribs.draw_args_stride = draw_args_stride;
  attribs.attribs_buffer_mode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          attribs_buffer_mode);
  attribs.counter_buffer =
      counter_buffer_object ? counter_buffer_object->AsRawPtr() : nullptr;
  attribs.counter_offset = counter_offset;
  attribs.counter_buffer_mode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          counter_buffer_mode);

  draw_attribs_ = std::move(attribs);
}

void MeshImpl::SetDrawAttribs(
    scoped_refptr<GPUBuffer> attribs_buffer,
    uint64_t draw_args_offset,
    uint32_t draw_count,
    uint32_t draw_args_stride,
    GPU::ResourceStateTransitionMode attribs_buffer_mode,
    scoped_refptr<GPUBuffer> counter_buffer,
    uint64_t counter_offset,
    GPU::ResourceStateTransitionMode counter_buffer_mode,
    GPU::ValueType index_type,
    ExceptionState& exception_state) {
  auto* attribs_buffer_object = static_cast<BufferImpl*>(attribs_buffer.get());
  auto* counter_buffer_object = static_cast<BufferImpl*>(counter_buffer.get());

  DrawIndexedIndirectAttribs attribs;
  attribs.attribs_buffer =
      attribs_buffer_object ? attribs_buffer_object->AsRawPtr() : nullptr;
  attribs.draw_args_offset = draw_args_offset;
  attribs.draw_count = draw_count;
  attribs.draw_args_stride = draw_args_stride;
  attribs.attribs_buffer_mode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          attribs_buffer_mode);
  attribs.counter_buffer =
      counter_buffer_object ? counter_buffer_object->AsRawPtr() : nullptr;
  attribs.counter_offset = counter_offset;
  attribs.counter_buffer_mode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          counter_buffer_mode);
  attribs.index_type = static_cast<Diligent::VALUE_TYPE>(index_type);

  draw_attribs_ = std::move(attribs);
}

void MeshImpl::SetMultiDrawAttribs(
    const std::vector<scoped_refptr<GPUMultiDrawItem>>& items,
    uint32_t num_instances,
    uint32_t first_instance,
    ExceptionState& exception_state) {
  MultiDrawAttribs attribs;
  for (auto& it : items) {
    Diligent::MultiDrawItem result;
    result.NumVertices = it->num_vertices;
    result.StartVertexLocation = it->start_vertex;
    attribs.items.push_back(std::move(result));
  }
  attribs.num_instances = num_instances;
  attribs.first_instance = first_instance;

  draw_attribs_ = std::move(attribs);
}

void MeshImpl::SetMultiDrawAttribs(
    const std::vector<scoped_refptr<GPUMultiDrawIndexedItem>>& items,
    GPU::ValueType index_type,
    uint32_t num_instances,
    uint32_t first_instance,
    ExceptionState& exception_state) {
  MultiDrawIndexedAttribs attribs;
  for (auto& it : items) {
    Diligent::MultiDrawIndexedItem result;
    result.NumIndices = it->num_indices;
    result.FirstIndexLocation = it->start_index;
    result.BaseVertex = it->base_vertex;
    attribs.items.push_back(std::move(result));
  }
  attribs.index_type = static_cast<Diligent::VALUE_TYPE>(index_type);
  attribs.num_instances = num_instances;
  attribs.first_instance = first_instance;

  draw_attribs_ = std::move(attribs);
}

scoped_refptr<Viewport> MeshImpl::Get_Viewport(
    ExceptionState& exception_state) {
  return viewport_;
}

void MeshImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                   : context()->screen_drawable_node);
}

bool MeshImpl::Get_Visible(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return node_.GetVisibility();
}

void MeshImpl::Put_Visible(const bool& value, ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeVisibility(value);
}

int32_t MeshImpl::Get_Z(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return node_.GetSortKeys()->weight[0];
}

void MeshImpl::Put_Z(const int32_t& value, ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeSortWeight(value);
}

scoped_refptr<GPUPipelineState> MeshImpl::Get_PipelineState(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return pipeline_state_;
}

void MeshImpl::Put_PipelineState(const scoped_refptr<GPUPipelineState>& value,
                                 ExceptionState& exception_state) {
  DISPOSE_CHECK;

  pipeline_state_ = static_cast<PipelineStateImpl*>(value.get());
}

scoped_refptr<GPUResourceBinding> MeshImpl::Get_ResourceBinding(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return resource_binding_;
}

void MeshImpl::Put_ResourceBinding(
    const scoped_refptr<GPUResourceBinding>& value,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  resource_binding_ = static_cast<ResourceBindingImpl*>(value.get());

  // Set world binding
  if (auto* binding = resource_binding_->AsRawPtr())
    world_variable_ = binding->GetVariableByName(
        Diligent::SHADER_TYPE_VERTEX, kShaderWorldMatrixVariableName);
}

void MeshImpl::OnObjectDisposed() {
  node_.DisposeNode();

  VertexBufferAttribs empty_vertex_buffer;
  std::swap(vertex_buffers_, empty_vertex_buffer);
  IndexBufferAttribs empty_index_buffer;
  std::swap(index_buffer_, empty_index_buffer);
  pipeline_state_.reset();
  resource_binding_.reset();
}

void MeshImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (stage == DrawableNode::ON_RENDERING) {
    // Execute rendering directly
    GPURenderInternal(params->context, params->world_binding);
  }
}

void MeshImpl::GPURenderInternal(Diligent::IDeviceContext* render_context,
                                 Diligent::IBuffer* world_buffer) {
  if (!pipeline_state_ || !resource_binding_)
    return;

  // Input Assembly
  std::vector<Diligent::IBuffer*> buffers(vertex_buffers_.buffers.size());
  for (uint32_t i = 0; i < buffers.size(); ++i)
    buffers[i] = vertex_buffers_.buffers[i].RawPtr();

  render_context->SetVertexBuffers(
      vertex_buffers_.start_slot, vertex_buffers_.buffers.size(),
      buffers.data(), vertex_buffers_.offsets.data(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      vertex_buffers_.flags);
  render_context->SetIndexBuffer(
      index_buffer_.index_buffer, index_buffer_.byte_offset,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Resource binding
  if (world_variable_)
    world_variable_->Set(world_buffer);
  render_context->SetPipelineState(pipeline_state_->AsRawPtr());
  render_context->CommitShaderResources(
      resource_binding_->AsRawPtr(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Draw call
  std::visit(
      [render_context](auto& attribs) {
        using AttribType = std::decay_t<decltype(attribs)>;
        if constexpr (std::is_same_v<AttribType, DrawAttribs>) {
          Diligent::DrawAttribs render_attribs;
          render_attribs.NumVertices = attribs.num_vertices;
          render_attribs.NumInstances = attribs.num_instances;
          render_attribs.StartVertexLocation = attribs.first_vertex;
          render_attribs.FirstInstanceLocation = attribs.first_instance;
          render_context->Draw(render_attribs);
        } else if constexpr (std::is_same_v<AttribType, DrawIndexedAttribs>) {
          Diligent::DrawIndexedAttribs render_attribs;
          render_attribs.NumIndices = attribs.num_indices;
          render_attribs.NumInstances = attribs.num_instances;
          render_attribs.FirstIndexLocation = attribs.first_index;
          render_attribs.BaseVertex = attribs.base_vertex;
          render_attribs.FirstInstanceLocation = attribs.first_instance;
          render_attribs.IndexType = attribs.index_type;
          render_context->DrawIndexed(render_attribs);
        } else if constexpr (std::is_same_v<AttribType, DrawIndirectAttribs>) {
          Diligent::DrawIndirectAttribs render_attribs;
          render_attribs.pAttribsBuffer = attribs.attribs_buffer;
          render_attribs.DrawArgsOffset = attribs.draw_args_offset;
          render_attribs.DrawCount = attribs.draw_count;
          render_attribs.DrawArgsStride = attribs.draw_args_stride;
          render_attribs.AttribsBufferStateTransitionMode =
              attribs.attribs_buffer_mode;
          render_attribs.pCounterBuffer = attribs.counter_buffer;
          render_attribs.CounterOffset = attribs.counter_offset;
          render_attribs.CounterBufferStateTransitionMode =
              attribs.counter_buffer_mode;
          render_context->DrawIndirect(render_attribs);
        } else if constexpr (std::is_same_v<AttribType,
                                            DrawIndexedIndirectAttribs>) {
          Diligent::DrawIndexedIndirectAttribs render_attribs;
          render_attribs.pAttribsBuffer = attribs.attribs_buffer;
          render_attribs.DrawArgsOffset = attribs.draw_args_offset;
          render_attribs.DrawCount = attribs.draw_count;
          render_attribs.DrawArgsStride = attribs.draw_args_stride;
          render_attribs.AttribsBufferStateTransitionMode =
              attribs.attribs_buffer_mode;
          render_attribs.pCounterBuffer = attribs.counter_buffer;
          render_attribs.CounterOffset = attribs.counter_offset;
          render_attribs.CounterBufferStateTransitionMode =
              attribs.counter_buffer_mode;
          render_attribs.IndexType = attribs.index_type;
          render_context->DrawIndexedIndirect(render_attribs);
        } else if constexpr (std::is_same_v<AttribType, MultiDrawAttribs>) {
          Diligent::MultiDrawAttribs render_attribs;
          render_attribs.pDrawItems = attribs.items.data();
          render_attribs.DrawCount = attribs.items.size();
          render_attribs.NumInstances = attribs.num_instances;
          render_attribs.FirstInstanceLocation = attribs.first_instance;
          render_context->MultiDraw(render_attribs);
        } else if constexpr (std::is_same_v<AttribType,
                                            MultiDrawIndexedAttribs>) {
          Diligent::MultiDrawIndexedAttribs render_attribs;
          render_attribs.pDrawItems = attribs.items.data();
          render_attribs.DrawCount = attribs.items.size();
          render_attribs.IndexType = attribs.index_type;
          render_attribs.NumInstances = attribs.num_instances;
          render_attribs.FirstInstanceLocation = attribs.first_instance;
          render_context->MultiDrawIndexed(render_attribs);
        } else {
          static_assert("unreachable attribs");
        }
      },
      draw_attribs_);
}

}  // namespace content
