// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/bind/callback.h"
#include "content/common/exception.h"
#include "content/common/object.h"
#include "content/content_config.h"
#include "content/gpu/gpu_resource.h"

namespace content {

URGE_BINDING()
class SubMesh : public Object {
 public:
  URGE_BINDING()
  uint32_t vertexSlot = 0;

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
  estring name;
};

URGE_BINDING()
class Mesh : public Object {
 public:
  Mesh();

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<Mesh> New(uint32_t vertex_bytes,
                                 uint32_t index_count,
                                 URGE_EXCEPTION);

  URGE_BINDING()
  epointer GetVertices(uint32_t slot, URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetVertexCount(uint32_t slot, URGE_EXCEPTION);

  URGE_BINDING()
  epointer GetIndices(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetIndexCount(URGE_EXCEPTION);

  URGE_BINDING()
  void SetupSubMeshData(earray<scoped_refptr<SubMesh>> data, URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<SubMesh> GetSubMeshAt(uint32_t index, URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetSubMeshCount(URGE_EXCEPTION);

 private:
};

}  // namespace content
