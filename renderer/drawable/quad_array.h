// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_DRAWABLE_QUAD_ARRAY_H_
#define RENDERER_DRAWABLE_QUAD_ARRAY_H_

#include "renderer/drawable/quad_drawable.h"

namespace renderer {

class QuadArray {
 public:
  using Vertex = GeometryVertexLayout;
  using VertexType = GeometryVertexLayout::Data;

  QuadArray(RefCntAutoPtr<IRenderDevice> device,
            scoped_refptr<QuadArrayIndices> indices);

  QuadArray(const QuadArray&) = delete;
  QuadArray& operator=(const QuadArray&) = delete;

  void Clear();
  void Resize(RefCntAutoPtr<IDeviceContext> context, size_t size);
  void Draw(RefCntAutoPtr<IDeviceContext> context, size_t offset, size_t count);
  void Draw(RefCntAutoPtr<IDeviceContext> context) {
    Draw(context, 0, quad_size_);
  }

  inline std::vector<VertexType>& vertices() { return vertices_; }
  inline size_t count() const { return quad_size_; }

 private:
  RefCntAutoPtr<IRenderDevice> device_;
  RefCntAutoPtr<IBuffer> buffer_;
  scoped_refptr<QuadArrayIndices> indices_;

  std::vector<VertexType> vertices_;
  size_t quad_size_;
  size_t buffer_size_;
};

}  // namespace renderer

#endif  //! RENDERER_DRAWABLE_QUAD_DRAWABLE_H_
