// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_QUERY_IMPL_H_
#define CONTENT_GPU_QUERY_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/Query.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpuquery.h"

namespace content {

class QueryImpl : public GPUQuery, public EngineObject, public Disposable {
 public:
  QueryImpl(ExecutionContext* context, Diligent::IQuery* object);
  ~QueryImpl() override;

  QueryImpl(const QueryImpl&) = delete;
  QueryImpl& operator=(const QueryImpl&) = delete;

  Diligent::IQuery* AsRawPtr() const { return object_; }

 protected:
  // GPUQuery interface
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  bool GetData(void* data,
               uint32_t size,
               bool auto_invalidate,
               ExceptionState& exception_state) override;
  void Invalidate(ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.Query"; }

  Diligent::RefCntAutoPtr<Diligent::IQuery> object_;
};

}  // namespace content

#endif  // !CONTENT_GPU_QUERY_IMPL_H_
