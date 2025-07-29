// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_RESOURCE_MAPPING_IMPL_H_
#define CONTENT_GPU_RESOURCE_MAPPING_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/ResourceMapping.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpuresourcemapping.h"

namespace content {

class ResourceMappingImpl : public GPUResourceMapping,
                            public EngineObject,
                            public Disposable {
 public:
  ResourceMappingImpl(ExecutionContext* context,
                      Diligent::IResourceMapping* object);
  ~ResourceMappingImpl() override;

  ResourceMappingImpl(const ResourceMappingImpl&) = delete;
  ResourceMappingImpl& operator=(const ResourceMappingImpl&) = delete;

  Diligent::IResourceMapping* AsRawPtr() const { return object_; }

 protected:
  // GPUResourceMapping interface
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  void AddResource(const std::string& name,
                   uint64_t device_object,
                   bool is_unique,
                   ExceptionState& exception_state) override;
  void AddResourceArray(const std::string& name,
                        uint32_t start_index,
                        const std::vector<uint64_t>& device_objects,
                        bool is_unique,
                        ExceptionState& exception_state) override;
  void RemoveResourceByName(const std::string& name,
                            uint32_t array_index,
                            ExceptionState& exception_state) override;
  uint64_t GetResource(const std::string& name,
                       uint32_t array_index,
                       ExceptionState& exception_state) override;
  uint64_t GetSize(ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.ResourceMapping"; }

  Diligent::RefCntAutoPtr<Diligent::IResourceMapping> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_RESOURCE_MAPPING_IMPL_H_
