// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUBUFFER_H_
#define CONTENT_PUBLIC_ENGINE_GPUBUFFER_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"

namespace content {

class GPUBuffer;

/*--urge(name:GPUBufferView)--*/
class URGE_OBJECT(GPUBufferView) {
 public:
  virtual ~GPUBufferView() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:device_object)--*/
  virtual uint64_t GetDeviceObject(ExceptionState& exception_state) = 0;

  /*--urge(name:desc)--*/
  virtual scoped_refptr<GPUBufferViewDesc> GetDesc(
      ExceptionState& exception_state) = 0;

  /*--urge(name:buffer)--*/
  virtual scoped_refptr<GPUBuffer> GetBuffer(
      ExceptionState& exception_state) = 0;
};

/*--urge(name:GPUBuffer)--*/
class URGE_OBJECT(GPUBuffer) {
 public:
  virtual ~GPUBuffer() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:device_object)--*/
  virtual uint64_t GetDeviceObject(ExceptionState& exception_state) = 0;

  /*--urge(name:desc)--*/
  virtual scoped_refptr<GPUBufferDesc> GetDesc(
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_view)--*/
  virtual scoped_refptr<GPUBufferView> CreateView(
      scoped_refptr<GPUBufferViewDesc> desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:default_view)--*/
  virtual scoped_refptr<GPUBufferView> GetDefaultView(
      GPU::BufferViewType view_type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:flush_mapped_range)--*/
  virtual void FlushMappedRange(uint64_t start_offset,
                                uint64_t size,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:invalidate_mapped_range)--*/
  virtual void InvalidateMappedRange(uint64_t start_offset,
                                     uint64_t size,
                                     ExceptionState& exception_state) = 0;

  /*--urge(name:state)--*/
  URGE_EXPORT_ATTRIBUTE(State, GPU::ResourceState);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUBUFFER_H_
