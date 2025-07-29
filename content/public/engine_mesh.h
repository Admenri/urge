// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_MESH_H_
#define CONTENT_PUBLIC_ENGINE_MESH_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_bitmap.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gpupipelinestate.h"
#include "content/public/engine_gpuresourcebinding.h"
#include "content/public/engine_viewport.h"

namespace content {

/*--urge(name:Mesh)--*/
class URGE_OBJECT(Mesh) {
 public:
  virtual ~Mesh() = default;

  /*--urge(name:initialize,optional:viewport=nullptr)--*/
  static scoped_refptr<Mesh> New(ExecutionContext* execution_context,
                                 scoped_refptr<Viewport> viewport,
                                 ExceptionState& exception_state);

  /*--urge(name:set_label)--*/
  virtual void SetLabel(const std::string& label,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:set_vertex_buffers)--*/
  virtual void SetVertexBuffers(
      uint32_t start_slot,
      const std::vector<scoped_refptr<GPUBuffer>>& buffers,
      const std::vector<uint64_t>& offsets,
      GPU::SetVertexBuffersFlags flags,
      ExceptionState& exception_state) = 0;

  /*--urge(name:set_index_buffer)--*/
  virtual void SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                              uint64_t byte_offset,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:set_draw_attribs)--*/
  virtual void SetDrawAttribs(uint32_t num_vertices,
                              uint32_t num_instances,
                              uint32_t first_vertex,
                              uint32_t first_instance,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:set_draw_attribs)--*/
  virtual void SetDrawAttribs(uint32_t num_indices,
                              uint32_t num_instances,
                              uint32_t first_index,
                              uint32_t base_vertex,
                              uint32_t first_instance,
                              GPU::ValueType index_type,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:set_draw_attribs)--*/
  virtual void SetDrawAttribs(
      scoped_refptr<GPUBuffer> attribs_buffer,
      uint64_t draw_args_offset,
      uint32_t draw_count,
      uint32_t draw_args_stride,
      GPU::ResourceStateTransitionMode attribs_buffer_mode,
      scoped_refptr<GPUBuffer> counter_buffer,
      uint64_t counter_offset,
      GPU::ResourceStateTransitionMode counter_buffer_mode,
      ExceptionState& exception_state) = 0;

  /*--urge(name:set_draw_attribs)--*/
  virtual void SetDrawAttribs(
      scoped_refptr<GPUBuffer> attribs_buffer,
      uint64_t draw_args_offset,
      uint32_t draw_count,
      uint32_t draw_args_stride,
      GPU::ResourceStateTransitionMode attribs_buffer_mode,
      scoped_refptr<GPUBuffer> counter_buffer,
      uint64_t counter_offset,
      GPU::ResourceStateTransitionMode counter_buffer_mode,
      GPU::ValueType index_type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:set_multi_draw_attribs)--*/
  virtual void SetMultiDrawAttribs(
      const std::vector<scoped_refptr<GPUMultiDrawItem>>& items,
      uint32_t num_instances,
      uint32_t first_instance,
      ExceptionState& exception_state) = 0;

  /*--urge(name:set_multi_draw_attribs)--*/
  virtual void SetMultiDrawAttribs(
      const std::vector<scoped_refptr<GPUMultiDrawIndexedItem>>& items,
      GPU::ValueType index_type,
      uint32_t num_instances,
      uint32_t first_instance,
      ExceptionState& exception_state) = 0;

  /*--urge(name:viewport)--*/
  URGE_EXPORT_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);

  /*--urge(name:visible)--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);

  /*--urge(name:z)--*/
  URGE_EXPORT_ATTRIBUTE(Z, int32_t);

  /*--urge(name:pipeline_state)--*/
  URGE_EXPORT_ATTRIBUTE(PipelineState, scoped_refptr<GPUPipelineState>);

  /*--urge(name:resource_binding)--*/
  URGE_EXPORT_ATTRIBUTE(ResourceBinding, scoped_refptr<GPUResourceBinding>);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_MESH_H_
