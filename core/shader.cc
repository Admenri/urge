// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/shader.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "SPIRV/GlslangToSpv.h"
#include "glslang/Public/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"

#include "spirv_reflect.h"

#include "core/exception.h"
#include "core/device.h"
#include "core/pipeline.h"

namespace urge {

namespace {

//! Runs the initialize/finalize of glslang once for the whole process.
class GlslangGuard {
 public:
  GlslangGuard() { glslang::InitializeProcess(); }
  ~GlslangGuard() { glslang::FinalizeProcess(); }
};

GlslangGuard& GetGlslang() {
  static GlslangGuard guard;
  return guard;
}

[[noreturn]] void Fail(std::string_view what, const std::string& detail) {
  throw Exception(Exception::kGPUError, "shader {} failed: {}", what, detail);
}

EShLanguage ToLanguage(wgpu::ShaderStage stage) {
  switch (stage) {
    case wgpu::ShaderStage::Vertex:
      return EShLangVertex;
    case wgpu::ShaderStage::Fragment:
      return EShLangFragment;
    default:
      throw Exception(Exception::kGPUError, "shader stage {} is not supported",
                      static_cast<uint64_t>(stage));
  }
}

//! Compiles the GLSL of one stage to the SPIR-V of it.
std::vector<uint32_t> CompileToSpirv(wgpu::ShaderStage stage,
                                     std::string_view glsl) {
  GetGlslang();

  const EShLanguage language = ToLanguage(stage);
  glslang::TShader shader(language);

  // glslang reads the source of a stage as a zero terminated string
  const std::string source(glsl);
  const char* source_ptr = source.c_str();
  shader.setStrings(&source_ptr, 1);

  shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan,
                     100);
  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

  // The module of the engine keeps its bindings where the GLSL puts them
  shader.setAutoMapBindings(false);
  shader.setAutoMapLocations(false);

  if (!shader.parse(GetDefaultResources(), 450, false, EShMsgDefault))
    Fail("compile", shader.getInfoLog());

  glslang::TProgram program;
  program.addShader(&shader);
  if (!program.link(EShMsgDefault))
    Fail("link", program.getInfoLog());

  std::vector<uint32_t> spirv;
  glslang::SpvOptions options;
  options.generateDebugInfo = false;
  glslang::GlslangToSpv(*program.getIntermediate(language), spirv, &options);
  if (spirv.empty())
    Fail("translate", "the compiler produced no SPIR-V");
  return spirv;
}

//! Reads the bindings and the vertex inputs of a SPIR-V module.
ShaderReflection ReflectSpirv(const std::vector<uint32_t>& spirv) {
  SpvReflectShaderModule module = {};
  const SpvReflectResult result = spvReflectCreateShaderModule(
      spirv.size() * sizeof(uint32_t), spirv.data(), &module);
  if (result != SPV_REFLECT_RESULT_SUCCESS)
    Fail("reflect", std::to_string(static_cast<int>(result)));

  ShaderReflection reflection;
  for (uint32_t set_index = 0; set_index < module.descriptor_set_count;
       ++set_index) {
    const SpvReflectDescriptorSet& set = module.descriptor_sets[set_index];
    while (reflection.groups.size() <= set.set)
      reflection.groups.emplace_back();

    ShaderGroup& group = reflection.groups[set.set];
    for (uint32_t binding_index = 0; binding_index < set.binding_count;
         ++binding_index) {
      const SpvReflectDescriptorBinding& binding = *set.bindings[binding_index];

      ShaderBinding entry;
      entry.binding = binding.binding;
      switch (binding.descriptor_type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          entry.buffer = WGPUBufferBindingType_Uniform;
          entry.min_binding_size = binding.block.size;
          break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          entry.buffer = WGPUBufferBindingType_Storage;
          entry.min_binding_size = binding.block.size;
          break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          entry.texture = true;
          break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
          entry.sampler = true;
          break;
        default:
          Fail("reflect", "a binding kind the engine does not use");
      }
      group.bindings.push_back(entry);
    }
  }

  for (uint32_t index = 0; index < module.input_variable_count; ++index) {
    const SpvReflectInterfaceVariable* variable = module.input_variables[index];
    // The builtins of a stage have no location, they are not attributes
    if (variable->location == static_cast<uint32_t>(-1))
      continue;

    ShaderAttribute attribute;
    attribute.location = variable->location;
    switch (variable->format) {
      case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
        attribute.format = WGPUVertexFormat_Float32x4;
        break;
      case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
        attribute.format = WGPUVertexFormat_Float32x3;
        break;
      case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32_SFLOAT:
        attribute.format = WGPUVertexFormat_Float32x2;
        break;
      case SpvReflectFormat::SPV_REFLECT_FORMAT_R32_SFLOAT:
        attribute.format = WGPUVertexFormat_Float32;
        break;
      default:
        Fail("reflect", "a vertex format the engine does not use");
    }
    reflection.attributes.push_back(attribute);
  }

  spvReflectDestroyShaderModule(&module);
  return reflection;
}

//! Creates the module of a stage from the WGSL the shader was translated to.
wgpu::ShaderModule CreateModule(const Shader& shader) {
  wgpu::ShaderModuleDescriptor module_desc;
  wgpu::ShaderSourceSPIRV spirv;
  spirv.code = shader.spirv().data();
  spirv.codeSize = shader.spirv().size();
  module_desc.nextInChain = &spirv;

  wgpu::ShaderModule module =
      GPUDevice::Get().device().CreateShaderModule(&module_desc);
  if (module == nullptr)
    Fail("module", "the device rejected the SPIRV of the shader");
  return module;
}

//! Fills one bind group layout entry from a binding of the reflection, with
//! \p dynamic_offset telling whether the set of the binding is bound with a
//! dynamic offset.
wgpu::BindGroupLayoutEntry MakeEntry(const ShaderBinding& binding,
                                     wgpu::ShaderStage visibility,
                                     bool dynamic_offset) {
  wgpu::BindGroupLayoutEntry entry;
  entry.binding = binding.binding;
  entry.visibility = visibility;

  if (binding.texture) {
    entry.texture.sampleType = wgpu::TextureSampleType::Float;
    entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
  } else if (binding.sampler) {
    entry.sampler.type = wgpu::SamplerBindingType::Filtering;
  } else {
    entry.buffer.type = static_cast<wgpu::BufferBindingType>(binding.buffer);
    entry.buffer.minBindingSize = binding.min_binding_size;
    entry.buffer.hasDynamicOffset = dynamic_offset;
  }
  return entry;
}

/**
\brief Creates the bind group layouts of both stages, indexed by their set.

The reflection describes the groups of a stage from the set a binding is in, so
the two stages are merged by the set number: a set both stages bind becomes one
layout which the two of them share, and a stage which skips a set leaves an
empty layout behind so the sets of the other stage keep their index.

\param[in] dynamic_sets The sets whose buffer bindings are bound with a dynamic
offset, they are the ones a bulk manager uploads one slot per object into.
*/
std::vector<wgpu::BindGroupLayout> CreateGroupLayouts(
    const Shader& vertex,
    const Shader& fragment,
    const std::set<uint32_t>& dynamic_sets,
    uint32_t min_sets) {
  const std::vector<ShaderGroup>& vertex_groups = vertex.reflection().groups;
  const std::vector<ShaderGroup>& fragment_groups =
      fragment.reflection().groups;

  std::vector<wgpu::BindGroupLayout> layouts;
  const size_t group_count =
      std::max({vertex_groups.size(), fragment_groups.size(),
                static_cast<size_t>(min_sets)});

  for (size_t group = 0; group < group_count; ++group) {
    // The entries of this set, keyed by the binding an entry is for
    std::map<uint32_t, wgpu::BindGroupLayoutEntry> entries;
    const bool dynamic_offset =
        dynamic_sets.contains(static_cast<uint32_t>(group));
    const auto add = [&entries, dynamic_offset](
                         const std::vector<ShaderBinding>& bindings,
                         wgpu::ShaderStage visibility) {
      for (const ShaderBinding& binding : bindings) {
        const auto found = entries.find(binding.binding);
        if (found == entries.end())
          entries.emplace(binding.binding,
                          MakeEntry(binding, visibility, dynamic_offset));
        else
          found->second.visibility |= visibility;
      }
    };

    if (group < vertex_groups.size())
      add(vertex_groups[group].bindings, wgpu::ShaderStage::Vertex);
    if (group < fragment_groups.size())
      add(fragment_groups[group].bindings, wgpu::ShaderStage::Fragment);

    std::vector<wgpu::BindGroupLayoutEntry> ordered;
    ordered.reserve(entries.size());
    for (const auto& entry : entries)
      ordered.push_back(entry.second);

    wgpu::BindGroupLayoutDescriptor layout_desc;
    layout_desc.entryCount = ordered.size();
    layout_desc.entries = ordered.empty() ? nullptr : ordered.data();

    wgpu::BindGroupLayout layout =
        GPUDevice::Get().device().CreateBindGroupLayout(&layout_desc);
    if (layout == nullptr)
      Fail("layout", "the device rejected a bind group layout of the shader");
    layouts.push_back(layout);
  }
  return layouts;
}

//! The size in bytes of a vertex format the reflection can report.
uint64_t FormatSize(WGPUVertexFormat format) {
  switch (format) {
    default:
    case WGPUVertexFormat_Uint32x2:
    case WGPUVertexFormat_Sint32x2:
    case WGPUVertexFormat_Float32x2:
      return 8;
    case WGPUVertexFormat_Uint32x3:
    case WGPUVertexFormat_Sint32x3:
    case WGPUVertexFormat_Float32x3:
      return 12;
    case WGPUVertexFormat_Uint32x4:
    case WGPUVertexFormat_Sint32x4:
    case WGPUVertexFormat_Float32x4:
      return 16;
  }
}

/*! The bindings of one set of a SPIR-V pair, merged by their binding: a
    binding both stages declare counts once, which is the set the pipeline
    layout is built from and the one a caller which fills the set reads. */
std::vector<ShaderBinding> MergeGroupBindings(
    const std::vector<ShaderGroup>& vertex_groups,
    const std::vector<ShaderGroup>& fragment_groups,
    uint32_t set) {
  std::map<uint32_t, ShaderBinding> merged;
  const auto add = [&merged, set](const std::vector<ShaderGroup>& groups) {
    if (set >= groups.size())
      return;
    for (const ShaderBinding& binding : groups[set].bindings)
      merged.emplace(binding.binding, binding);
  };
  add(vertex_groups);
  add(fragment_groups);

  std::vector<ShaderBinding> bindings;
  bindings.reserve(merged.size());
  for (const auto& entry : merged)
    bindings.push_back(entry.second);
  return bindings;
}

}  // namespace

Shader Shader::Compile(wgpu::ShaderStage stage, std::string_view glsl) {
  const std::vector<uint32_t> spirv = CompileToSpirv(stage, glsl);

  Shader shader;
  shader.reflection_ = ReflectSpirv(spirv);
  shader.spirv_ = spirv;
  // The reader names the entry point after the SPIR-V entry point
  shader.entry_point_ = "main";
  return shader;
}

/* ----- Pipeline ----- */

Pipeline::Pipeline(std::string_view vs_glsl,
                   std::string_view fs_glsl,
                   std::vector<std::vector<uint32_t>> vb_layouts,
                   const std::set<uint32_t>& dynamic_sets,
                   uint32_t min_sets)
    : vertex_(Shader::Compile(wgpu::ShaderStage::Vertex, vs_glsl)),
      fragment_(Shader::Compile(wgpu::ShaderStage::Fragment, fs_glsl)),
      dynamic_sets_(dynamic_sets) {
  const wgpu::Device device = GPUDevice::Get().device();

  vertex_module_ = CreateModule(vertex_);
  fragment_module_ = CreateModule(fragment_);

  /* The pipeline layout is written from the reflection instead of leaving it
     to the automatic mode of the device, so a bind group the engine makes from
     it always fits the pipeline. */
  const std::vector<wgpu::BindGroupLayout> group_layouts =
      CreateGroupLayouts(vertex_, fragment_, dynamic_sets_, min_sets);

  wgpu::PipelineLayoutDescriptor layout_desc;
  layout_desc.bindGroupLayoutCount = group_layouts.size();
  layout_desc.bindGroupLayouts =
      group_layouts.empty() ? nullptr : group_layouts.data();
  layout_ = device.CreatePipelineLayout(&layout_desc);
  if (layout_ == nullptr)
    Fail("layout", "the device rejected the pipeline layout of the shader");

  /* The vertex input is the one the vertex stage reads, which the reflection
     describes; vb_layouts says which of its locations share one buffer. The
     attributes of a buffer are packed in the order of their location, which is
     the order the vertex stage reads them in. */
  std::map<uint32_t, size_t> slot_of_location;
  for (size_t slot = 0; slot < vb_layouts.size(); ++slot) {
    for (const uint32_t location : vb_layouts[slot])
      slot_of_location.emplace(location, slot);
  }

  std::vector<ShaderAttribute> attributes = vertex_.reflection().attributes;
  std::sort(attributes.begin(), attributes.end(),
            [](const ShaderAttribute& left, const ShaderAttribute& right) {
              return left.location < right.location;
            });

  buffers_.resize(vb_layouts.size());
  for (const ShaderAttribute& shader_attribute : attributes) {
    const auto found = slot_of_location.find(shader_attribute.location);
    if (found == slot_of_location.end())
      Fail("vertex", "location " + std::to_string(shader_attribute.location) +
                         " is not fed by a vertex buffer layout");

    VertexBuffer& buffer = buffers_[found->second];
    wgpu::VertexAttribute attribute;
    attribute.format = static_cast<wgpu::VertexFormat>(shader_attribute.format);
    attribute.offset = buffer.stride;
    attribute.shaderLocation = shader_attribute.location;
    buffer.attributes.push_back(attribute);
    buffer.stride += FormatSize(shader_attribute.format);

    slot_of_location.erase(found);
  }

  if (!slot_of_location.empty())
    Fail("vertex", "location " +
                       std::to_string(slot_of_location.begin()->first) +
                       " is not read by the vertex shader");
}

wgpu::RenderPipeline Pipeline::MakeState(
    wgpu::PrimitiveState primitive,
    std::optional<wgpu::DepthStencilState> depth,
    std::vector<wgpu::ColorTargetState> blends,
    wgpu::MultisampleState samples) {
  /* The descriptor of a device call only has to live through the call, so the
     vertex input is written from what the constructor read. */
  std::vector<wgpu::VertexBufferLayout> buffers;
  buffers.reserve(buffers_.size());
  for (const VertexBuffer& buffer : buffers_) {
    wgpu::VertexBufferLayout layout;
    layout.stepMode = wgpu::VertexStepMode::Vertex;
    layout.arrayStride = buffer.stride;
    layout.attributeCount = buffer.attributes.size();
    layout.attributes =
        buffer.attributes.empty() ? nullptr : buffer.attributes.data();
    buffers.push_back(layout);
  }

  wgpu::VertexState vertex_state;
  vertex_state.module = vertex_module_;
  vertex_state.entryPoint = std::string_view(vertex_.entry_point());
  vertex_state.bufferCount = buffers.size();
  vertex_state.buffers = buffers.empty() ? nullptr : buffers.data();

  wgpu::FragmentState fragment_state;
  fragment_state.module = fragment_module_;
  fragment_state.entryPoint = std::string_view(fragment_.entry_point());
  fragment_state.targetCount = blends.size();
  fragment_state.targets = blends.empty() ? nullptr : blends.data();

  wgpu::RenderPipelineDescriptor descriptor;
  descriptor.layout = layout_;
  descriptor.vertex = vertex_state;
  descriptor.primitive = primitive;
  descriptor.depthStencil = depth ? &*depth : nullptr;
  descriptor.multisample = samples;
  descriptor.fragment = &fragment_state;

  wgpu::RenderPipeline pipeline =
      GPUDevice::Get().device().CreateRenderPipeline(&descriptor);
  if (pipeline == nullptr)
    Fail("state", "the device rejected the state of the render pipeline");

  return pipeline;
}

wgpu::RenderPipeline Pipeline::MakeDefaultState(const wgpu::BlendState* blend) {
  wgpu::ColorTargetState target;
  target.format = kColorTargetFormat;
  target.blend = blend;

  return MakeState(GetDefaultPrimitiveState(), *GetDepthStencilState(),
                   {target});
}

std::vector<ShaderBinding> Pipeline::group_bindings(uint32_t set) const {
  return MergeGroupBindings(vertex_.reflection().groups,
                            fragment_.reflection().groups, set);
}

}  // namespace urge
