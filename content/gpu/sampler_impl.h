// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_SAMPLER_H_
#define CONTENT_GPU_SAMPLER_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/Sampler.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpusampler.h"

namespace content {

class SamplerImpl : public GPUSampler, public EngineObject, public Disposable {
 public:
  SamplerImpl(ExecutionContext* context, Diligent::ISampler* object);
  ~SamplerImpl() override;

  SamplerImpl(const SamplerImpl&) = delete;
  SamplerImpl& operator=(const SamplerImpl&) = delete;

  Diligent::ISampler* AsRawPtr() const { return object_; }

 protected:
  // GPUSampler interface
  URGE_DECLARE_DISPOSABLE;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.Sampler"; }

  Diligent::RefCntAutoPtr<Diligent::ISampler> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_SAMPLER_H_
