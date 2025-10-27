// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURESOURCEMAPPING_H_
#define CONTENT_PUBLIC_ENGINE_GPURESOURCEMAPPING_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"

namespace content {

/*--urge(name:GPUResourceMapping)--*/
class URGE_OBJECT(GPUResourceMapping) {
 public:
  virtual ~GPUResourceMapping() = default;

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:add_resource)--*/
  virtual void AddResource(const std::string& name,
                           uint64_t device_object,
                           bool is_unique,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:add_resource_array)--*/
  virtual void AddResourceArray(const std::string& name,
                                uint32_t start_index,
                                const std::vector<uint64_t>& device_objects,
                                bool is_unique,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:remove_resource_by_name)--*/
  virtual void RemoveResourceByName(const std::string& name,
                                    uint32_t array_index,
                                    ExceptionState& exception_state) = 0;

  /*--urge(name:resource)--*/
  virtual uint64_t GetResource(const std::string& name,
                               uint32_t array_index,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:size)--*/
  virtual uint64_t GetSize(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURESOURCEMAPPING_H_
