// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_MESH2D_IMPL_H_
#define CONTENT_RENDER_MESH2D_IMPL_H_

#include <variant>

#include "content/canvas/canvas_impl.h"
#include "content/gpu/buffer_impl.h"
#include "content/gpu/pipeline_state_impl.h"
#include "content/gpu/resource_binding_impl.h"
#include "content/public/engine_mesh2d.h"
#include "content/public/engine_plane.h"
#include "content/render/drawable_controller.h"
#include "content/screen/viewport_impl.h"
#include "renderer/resource/render_buffer.h"

namespace content {

class Mesh2DImpl : public Mesh2D, public EngineObject, public Disposable {
 public:
  Mesh2DImpl(ExecutionContext* execution_context,
             scoped_refptr<ViewportImpl> parent);
  ~Mesh2DImpl() override;

  Mesh2DImpl(const Mesh2DImpl&) = delete;
  Mesh2DImpl& operator=(const Mesh2DImpl&) = delete;

 protected:
  void SetLabel(const base::String& label,
                ExceptionState& exception_state) override;
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  void SetVertexBuffers(uint32_t start_slot,
                        const base::Vector<scoped_refptr<GPUBuffer>>& buffers,
                        const base::Vector<uint64_t>& offsets,
                        GPU::SetVertexBuffersFlags flags,
                        ExceptionState& exception_state) override;
  void SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                      uint64_t byte_offset,
                      ExceptionState& exception_state) override;
  void SetDrawAttribs(uint32_t num_vertices,
                      uint32_t num_instances,
                      uint32_t first_vertex,
                      uint32_t first_instance,
                      ExceptionState& exception_state) override;
  void SetDrawAttribs(uint32_t num_indices,
                      uint32_t num_instances,
                      uint32_t first_index,
                      uint32_t base_vertex,
                      uint32_t first_instance,
                      GPU::ValueType index_type,
                      ExceptionState& exception_state) override;
  void SetDrawAttribs(scoped_refptr<GPUBuffer> attribs_buffer,
                      uint64_t draw_args_offset,
                      uint32_t draw_count,
                      uint32_t draw_args_stride,
                      GPU::ResourceStateTransitionMode attribs_buffer_mode,
                      scoped_refptr<GPUBuffer> counter_buffer,
                      uint64_t counter_offset,
                      GPU::ResourceStateTransitionMode counter_buffer_mode,
                      ExceptionState& exception_state) override;
  void SetDrawAttribs(scoped_refptr<GPUBuffer> attribs_buffer,
                      uint64_t draw_args_offset,
                      uint32_t draw_count,
                      uint32_t draw_args_stride,
                      GPU::ResourceStateTransitionMode attribs_buffer_mode,
                      scoped_refptr<GPUBuffer> counter_buffer,
                      uint64_t counter_offset,
                      GPU::ResourceStateTransitionMode counter_buffer_mode,
                      GPU::ValueType index_type,
                      ExceptionState& exception_state) override;
  void SetMultiDrawAttribs(
      const base::Vector<scoped_refptr<GPUMultiDrawItem>>& items,
      uint32_t num_instances,
      uint32_t first_instance,
      ExceptionState& exception_state) override;
  void SetMultiDrawAttribs(
      const base::Vector<scoped_refptr<GPUMultiDrawIndexedItem>>& items,
      GPU::ValueType index_type,
      uint32_t num_instances,
      uint32_t first_instance,
      ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Z, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(PipelineState,
                                  scoped_refptr<GPUPipelineState>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ResourceBinding,
                                  scoped_refptr<GPUResourceBinding>);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "Mesh2D"; }
  void DrawableNodeHandlerInternal(
      DrawableNode::RenderStage stage,
      DrawableNode::RenderControllerParams* params);

  void GPURenderInternal(Diligent::IDeviceContext* render_context,
                         Diligent::IBuffer* world_buffer);

  struct VertexBufferAttribs {
    uint32_t start_slot = 0;
    base::Vector<RRefPtr<Diligent::IBuffer>> buffers;
    base::Vector<uint64_t> offsets;
    Diligent::SET_VERTEX_BUFFERS_FLAGS flags =
        Diligent::SET_VERTEX_BUFFERS_FLAG_NONE;
  };

  struct IndexBufferAttribs {
    RRefPtr<Diligent::IBuffer> index_buffer;
    uint64_t byte_offset = 0;
  };

  struct DrawAttribs {
    uint32_t num_vertices;
    uint32_t num_instances;
    uint32_t first_vertex;
    uint32_t first_instance;
  };

  struct DrawIndexedAttribs {
    uint32_t num_indices;
    uint32_t num_instances;
    uint32_t first_index;
    uint32_t base_vertex;
    uint32_t first_instance;
    Diligent::VALUE_TYPE index_type;
  };

  struct DrawIndirectAttribs {
    RRefPtr<Diligent::IBuffer> attribs_buffer;
    uint64_t draw_args_offset;
    uint32_t draw_count;
    uint32_t draw_args_stride;
    Diligent::RESOURCE_STATE_TRANSITION_MODE attribs_buffer_mode;
    RRefPtr<Diligent::IBuffer> counter_buffer;
    uint64_t counter_offset;
    Diligent::RESOURCE_STATE_TRANSITION_MODE counter_buffer_mode;
  };

  struct DrawIndexedIndirectAttribs {
    RRefPtr<Diligent::IBuffer> attribs_buffer;
    uint64_t draw_args_offset;
    uint32_t draw_count;
    uint32_t draw_args_stride;
    Diligent::RESOURCE_STATE_TRANSITION_MODE attribs_buffer_mode;
    RRefPtr<Diligent::IBuffer> counter_buffer;
    uint64_t counter_offset;
    Diligent::RESOURCE_STATE_TRANSITION_MODE counter_buffer_mode;
    Diligent::VALUE_TYPE index_type;
  };

  struct MultiDrawAttribs {
    base::Vector<Diligent::MultiDrawItem> items;
    uint32_t num_instances;
    uint32_t first_instance;
  };

  struct MultiDrawIndexedAttribs {
    base::Vector<Diligent::MultiDrawIndexedItem> items;
    Diligent::VALUE_TYPE index_type;
    uint32_t num_instances;
    uint32_t first_instance;
  };

  DrawableNode node_;
  scoped_refptr<ViewportImpl> viewport_;

  VertexBufferAttribs vertex_buffers_;
  IndexBufferAttribs index_buffer_;
  scoped_refptr<PipelineStateImpl> pipeline_state_;
  scoped_refptr<ResourceBindingImpl> resource_binding_;

  RRefPtr<Diligent::IShaderResourceVariable> world_variable_;

  std::variant<DrawAttribs,
               DrawIndexedAttribs,
               DrawIndirectAttribs,
               DrawIndexedIndirectAttribs,
               MultiDrawAttribs,
               MultiDrawIndexedAttribs>
      draw_attribs_;
};

}  // namespace content

#endif  //! CONTENT_RENDER_MESH2D_IMPL_H_
