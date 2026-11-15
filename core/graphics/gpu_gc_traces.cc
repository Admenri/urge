// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.

#include "core/graphics/gpu.h"

namespace urge {

void GPUBindGroupDescriptor::Trace(GCVisitor& visitor) const {
  GPUObject::Trace(visitor);
  visitor.Trace(layout);
}

void GPUComputePipelineDescriptor::Trace(GCVisitor& visitor) const {
  GPUObject::Trace(visitor);
  visitor.Trace(layout);
  visitor.Trace(module);
}

void GPURenderPipelineDescriptor::Trace(GCVisitor& visitor) const {
  GPUObject::Trace(visitor);
  visitor.Trace(layout);
  visitor.Trace(vertex_module);
  visitor.Trace(fragment_module);
}

void GPURenderPassDescriptor::ColorAttachment::Trace(GCVisitor& visitor) const {
  visitor.Trace(view);
  visitor.Trace(resolve);
}

void GPURenderPassDescriptor::Trace(GCVisitor& visitor) const {
  GPUObject::Trace(visitor);
  visitor.Trace(color_attachments);
  visitor.Trace(depth_view);
  visitor.Trace(occlusion_query_set);
}

}
