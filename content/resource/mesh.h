// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/bind/callback.h"
#include "content/common/exception.h"
#include "content/common/object.h"
#include "content/common/vector.h"
#include "content/content_config.h"
#include "content/gpu/gpu_resource.h"

namespace content {

URGE_BINDING()
class SubMesh : public Object {
 public:
  URGE_BINDING()
  uint32_t bindingSlot = 0;

  URGE_BINDING()
  uint32_t materialSlot = 0;

  URGE_BINDING()
  uint32_t indexStart = 0;

  URGE_BINDING()
  uint32_t indexCount = 0;

  URGE_BINDING()
  uint32_t vertexStart = 0;

  URGE_BINDING()
  uint32_t vertexCount = 0;

  URGE_BINDING()
  scoped_refptr<Vector3> boundsMin = nullptr;

  URGE_BINDING()
  scoped_refptr<Vector3> boundsMax = nullptr;

  URGE_BINDING()
  estring name;
};

URGE_BINDING()
class Mesh : public Object {
 public:
  Mesh(uint32_t vertex_bytes, uint32_t index_count);

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  void UpdateGPUBuffer();

  wgpu::Buffer& vertex_buffer() { return vertex_buffer_; }
  wgpu::Buffer& index_buffer() { return index_buffer_; }

  const std::vector<scoped_refptr<SubMesh>>& mesh_group() const {
    return mesh_groups_;
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Mesh> New(uint32_t vertex_bytes,
                                 uint32_t index_count,
                                 URGE_EXCEPTION);

  URGE_BINDING()
  epointer GetVertices(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetVertexBytesSize(URGE_EXCEPTION);

  URGE_BINDING()
  epointer GetIndices(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetIndexCount(URGE_EXCEPTION);

  URGE_BINDING()
  void SetupSubMeshData(earray<scoped_refptr<SubMesh>> data, URGE_EXCEPTION);

  URGE_BINDING()
  earray<scoped_refptr<SubMesh>> GetSubMeshes(URGE_EXCEPTION);

 private:
  std::vector<scoped_refptr<SubMesh>> mesh_groups_;

  std::vector<uint8_t> vertices_;
  std::vector<uint32_t> indices_;

  wgpu::Buffer vertex_buffer_;
  wgpu::Buffer index_buffer_;
};

}  // namespace content
