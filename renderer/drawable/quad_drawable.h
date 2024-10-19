// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_QUAD_DRAWABLE_H_
#define RENDERER_QUAD_DRAWABLE_H_

#include "base/memory/ref_counted.h"
#include "renderer/drawable/vertex_layout.h"

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"

#include <vector>

namespace renderer {

using namespace Diligent;

class QuadArrayIndices : public base::RefCounted<QuadArrayIndices> {
 public:
  QuadArrayIndices(RefCntAutoPtr<IRenderDevice> device);

  QuadArrayIndices(const QuadArrayIndices&) = delete;
  QuadArrayIndices& operator=(const QuadArrayIndices&) = delete;

  void EnsureSize(RefCntAutoPtr<IDeviceContext> context, size_t count);
  RefCntAutoPtr<IBuffer> GetBufferHandle() const { return handle_; }

 private:
  RefCntAutoPtr<IRenderDevice> device_;
  RefCntAutoPtr<IBuffer> handle_;
  std::vector<uint16_t> buffer_;
};

class QuadDrawable {
 public:
  using VertexInput = GeometryVertexLayout;

  QuadDrawable(RefCntAutoPtr<IRenderDevice> device,
               scoped_refptr<QuadArrayIndices> indices);

  QuadDrawable(const QuadDrawable&) = delete;
  QuadDrawable& operator=(const QuadDrawable&) = delete;

  void SetPosition(const base::RectF& pos);
  void SetTexcoord(const base::RectF& tex);
  void SetColor(const base::Vec4& color, int index = -1);

  void Draw(RefCntAutoPtr<IDeviceContext> context);

 private:
  VertexInput::Data raw_data_[4];
  scoped_refptr<QuadArrayIndices> indices_;
  RefCntAutoPtr<IBuffer> buffer_handle_;
};

}  // namespace renderer

#endif  //! RENDERER_QUAD_DRAWABLE_H_
