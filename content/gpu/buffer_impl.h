// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_BUFFER_IMPL_H_
#define CONTENT_GPU_BUFFER_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/BufferView.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpubuffer.h"

namespace content {

class BufferViewImpl : public GPUBufferView,
                       public EngineObject,
                       public Disposable {
 public:
  BufferViewImpl(ExecutionContext* context, Diligent::IBufferView* object);
  ~BufferViewImpl() override;

  BufferViewImpl(const BufferViewImpl&) = delete;
  BufferViewImpl& operator=(const BufferViewImpl&) = delete;

  Diligent::IBufferView* AsRawPtr() const { return object_; }

 protected:
  URGE_DECLARE_DISPOSABLE;
  uint64_t GetDeviceObject(ExceptionState& exception_state) override;
  scoped_refptr<GPUBufferViewDesc> GetDesc(
      ExceptionState& exception_state) override;
  scoped_refptr<GPUBuffer> GetBuffer(ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.BufferView"; }

  Diligent::RefCntAutoPtr<Diligent::IBufferView> object_;
};

class BufferImpl : public GPUBuffer, public EngineObject, public Disposable {
 public:
  BufferImpl(ExecutionContext* context, Diligent::IBuffer* object);
  ~BufferImpl() override;

  BufferImpl(const BufferImpl&) = delete;
  BufferImpl& operator=(const BufferImpl&) = delete;

  Diligent::IBuffer* AsRawPtr() const { return object_; }

 protected:
  URGE_DECLARE_DISPOSABLE;
  uint64_t GetDeviceObject(ExceptionState& exception_state) override;
  scoped_refptr<GPUBufferDesc> GetDesc(
      ExceptionState& exception_state) override;
  scoped_refptr<GPUBufferView> CreateView(
      scoped_refptr<GPUBufferViewDesc> desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUBufferView> GetDefaultView(
      GPU::BufferViewType view_type,
      ExceptionState& exception_state) override;
  void FlushMappedRange(uint64_t start_offset,
                        uint64_t size,
                        ExceptionState& exception_state) override;
  void InvalidateMappedRange(uint64_t start_offset,
                             uint64_t size,
                             ExceptionState& exception_state) override;
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(State, GPU::ResourceState);

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.Buffer"; }

  Diligent::RefCntAutoPtr<Diligent::IBuffer> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_BUFFER_IMPL_H_
