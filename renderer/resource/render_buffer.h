// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_RESOURCE_RENDER_BUFFER_H_
#define RENDERER_RESOURCE_RENDER_BUFFER_H_

#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"

#include "renderer/layout/vertex_layout.h"
#include "renderer/renderer_config.h"

namespace renderer {

class QuadIndexCache {
 public:
  // Make quad index(6) buffer cache
  static QuadIndexCache Make(RRefPtr<Diligent::IRenderDevice> device,
                             Diligent::VALUE_TYPE type = Diligent::VT_UINT16) {
    return QuadIndexCache(device, type);
  }

  QuadIndexCache(const QuadIndexCache&) = delete;
  QuadIndexCache& operator=(const QuadIndexCache&) = delete;

  // Allocate a new capacity index buffer for drawcall using,
  // |quadrangle_size| is the count not the byte size.
  void Allocate(size_t quadrangle_size);

  // Current using index type
  Diligent::VALUE_TYPE GetIndexType() const { return type_; }

  bool operator()() const { return buffer_; }
  Diligent::IBuffer* operator*() { return buffer_; }

 private:
  QuadIndexCache(Diligent::IRenderDevice* device, Diligent::VALUE_TYPE type);

  Diligent::VALUE_TYPE type_;
  size_t index_count_;
  std::vector<uint8_t> cache_;

  RRefPtr<Diligent::IRenderDevice> device_;
  RRefPtr<Diligent::IBuffer> buffer_;
};

template <typename TargetType,
          Diligent::BIND_FLAGS BatchBind,
          Diligent::BUFFER_MODE BufferMode = Diligent::BUFFER_MODE_UNDEFINED,
          Diligent::CPU_ACCESS_FLAGS CPUAccess = Diligent::CPU_ACCESS_NONE,
          Diligent::USAGE Usage = Diligent::USAGE_DEFAULT,
          size_t ElementCount = 1>
class BatchBuffer {
 public:
  BatchBuffer() = default;

  BatchBuffer(const BatchBuffer&) = default;
  BatchBuffer& operator=(const BatchBuffer&) = default;

  bool operator()() const { return buffer_; }
  Diligent::IBuffer* operator*() { return buffer_; }

  static BatchBuffer Make(Diligent::IRenderDevice* device,
                          size_t initial_count = 0) {
    RRefPtr<Diligent::IBuffer> buffer;

    if (initial_count > 0) {
      Diligent::BufferDesc buffer_desc;
      MakeBufferDesc(buffer_desc, initial_count * sizeof(TargetType));

      device->CreateBuffer(buffer_desc, nullptr, &buffer);
    }

    return BatchBuffer(device, buffer);
  }

  void QueueWrite(Diligent::IDeviceContext* context,
                  const TargetType* data,
                  size_t count = 1) {
    if (!count)
      return;

    size_t bytes_size = count * sizeof(TargetType);
    if (!buffer_ || buffer_->GetDesc().Size < bytes_size) {
      Diligent::BufferDesc buffer_desc;
      MakeBufferDesc(buffer_desc, bytes_size);

      buffer_.Release();
      device_->CreateBuffer(buffer_desc, nullptr, &buffer_);
    }

    if (!data)
      return;

    if constexpr (Usage == Diligent::USAGE_DEFAULT) {
      context->UpdateBuffer(
          buffer_, 0, bytes_size, data,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    } else {
      void* mapping_buffer = nullptr;
      context->MapBuffer(buffer_, Diligent::MAP_WRITE,
                         Diligent::MAP_FLAG_DISCARD, mapping_buffer);
      std::memcpy(mapping_buffer, data, bytes_size);
      context->UnmapBuffer(buffer_, Diligent::MAP_WRITE);
    }
  }

 private:
  BatchBuffer(Diligent::IRenderDevice* device, Diligent::IBuffer* buffer)
      : device_(device), buffer_(buffer) {}

  static void MakeBufferDesc(Diligent::BufferDesc& buffer_desc,
                             size_t bytes_size) {
    buffer_desc.Name = typeid(TargetType).name();
    buffer_desc.Size = bytes_size;
    buffer_desc.BindFlags = BatchBind;
    buffer_desc.Usage = Usage;
    buffer_desc.CPUAccessFlags = CPUAccess;
    buffer_desc.Mode = BufferMode;
    buffer_desc.ElementByteStride = sizeof(TargetType) / ElementCount;
  }

  RRefPtr<Diligent::IRenderDevice> device_;
  RRefPtr<Diligent::IBuffer> buffer_;
};

using QuadBatch = BatchBuffer<Quad,
                              Diligent::BIND_VERTEX_BUFFER,
                              Diligent::BUFFER_MODE_UNDEFINED,
                              Diligent::CPU_ACCESS_NONE,
                              Diligent::USAGE_DEFAULT,
                              4>;

using DynamicQuadBatch = BatchBuffer<Quad,
                                     Diligent::BIND_VERTEX_BUFFER,
                                     Diligent::BUFFER_MODE_UNDEFINED,
                                     Diligent::CPU_ACCESS_WRITE,
                                     Diligent::USAGE_DYNAMIC,
                                     4>;

}  // namespace renderer

#endif  //! RENDERER_RESOURCE_RENDER_BUFFER_H_
