// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_BUFFER_IMPL_H_
#define CONTENT_GPU_BUFFER_IMPL_H_

#include "content/public/engine_gpubuffer.h"

namespace content {

class BufferImpl : public GPUBuffer {
 public:
  BufferImpl();
  ~BufferImpl() override;

  BufferImpl(const BufferImpl&) = delete;
  BufferImpl& operator=(const BufferImpl&) = delete;


};

class BufferViewImpl : public GPUBufferView {
 public:
  BufferViewImpl();
  ~BufferViewImpl() override;
};

}  // namespace content

#endif  //! CONTENT_GPU_BUFFER_IMPL_H_
