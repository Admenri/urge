// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/resource/mesh.h"

namespace content {

Mesh::Mesh(uint32_t vertex_bytes, uint32_t index_count) {
  vertices_.assign(vertex_bytes, 0);
  indices_.assign(index_count, 0);
}

scoped_refptr<Mesh> Mesh::New(uint32_t vertex_bytes,
                              uint32_t index_count,
                              URGE_EXCEPTION) {
  return Object::Create<Mesh>(vertex_bytes, index_count);
}

epointer Mesh::GetVertices(URGE_EXCEPTION) {
  return vertices_.data();
}

uint32_t Mesh::GetVertexBytesSize(URGE_EXCEPTION) {
  return vertices_.size();
}

epointer Mesh::GetIndices(URGE_EXCEPTION) {
  return indices_.data();
}

uint32_t Mesh::GetIndexCount(URGE_EXCEPTION) {
  return indices_.size();
}

void Mesh::SetupSubMeshData(earray<scoped_refptr<SubMesh>> data,
                            URGE_EXCEPTION) {
  mesh_groups_ = data;
}

earray<scoped_refptr<SubMesh>> Mesh::GetSubMeshes(URGE_EXCEPTION) {
  return mesh_groups_;
}

}  // namespace content
