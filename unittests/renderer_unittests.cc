
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#undef ENGINE_DLL

#include "BasicMath.hpp"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "Graphics/GraphicsTools/interface/GraphicsUtilities.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include "TextureLoader/interface/TextureUtilities.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "base/debug/logging.h"
#include "base/math/rectangle.h"
#include "renderer/device/render_device.h"
#include "renderer/drawable/quad_array.h"
#include "renderer/drawable/quad_drawable.h"
#include "ui/widget/widget.h"

#include <iostream>
#include <random>

using namespace Diligent;

inline void MakeProjectionMatrix(float* out,
                                 const base::Vec2& size,
                                 bool origin_bottom) {
  const float aa = 2.0f / size.x;
  const float bb = (origin_bottom ? 2.0f : -2.0f) / size.y;
  const float cc = origin_bottom ? 2.0f : 1.0f;
  const float dd = -1.0f;
  const float ee = origin_bottom ? -1.0f : 1.0f;
  const float ff = origin_bottom ? -1.0f : 0.0f;

  memset(out, 0, sizeof(float) * 16);
  out[0] = aa;
  out[5] = bb;
  out[10] = cc;

  out[3] = dd;
  out[7] = ee;
  out[11] = ff;
  out[15] = 1.0f;
}

struct Vertex {
  base::Vec3 pos;
  base::Vec2 uv;
};

struct CBConstants {
  float proj_mat[16];
  base::Vec2 tex_size;
};

static const char* VSSource = R"(
cbuffer Constants { 
  float4x4 g_WorldViewProj;
  float2 g_TexSize;
};

struct VSInput { 
  float3 Pos : ATTRIB0;
  float2 UV  : ATTRIB1;
};

struct PSInput { 
  float4 Pos   : SV_POSITION; 
  float2 UV    : TEX_COORD;
};

void main(in  VSInput VSIn,
          out PSInput PSIn) {
  PSIn.Pos = mul(g_WorldViewProj, float4(10.0, 10.0, 0.0, 0.0) + float4(VSIn.Pos, 1.0));
  PSIn.UV = float2(g_TexSize.x * VSIn.UV.x, g_TexSize.y * VSIn.UV.y);
}
)";

static const char* PSSource = R"(
Texture2D    g_Texture;
SamplerState g_Texture_sampler;

struct PSInput { 
  float4 Pos   : SV_POSITION; 
  float2 UV    : TEX_COORD; 
};

struct PSOutput { 
  float4 Color : SV_TARGET; 
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut) { 
  PSOut.Color = g_Texture.Sample(g_Texture_sampler, PSIn.UV);
}
)";

void set_rect(Vertex* vert, base::RectF pos, base::RectF tex) {
  vert[0].pos.x = pos.x;
  vert[0].pos.y = pos.y;
  vert[1].pos.x = pos.x + pos.width;
  vert[1].pos.y = pos.y;
  vert[2].pos.x = pos.x + pos.width;
  vert[2].pos.y = pos.y + pos.height;
  vert[3].pos.x = pos.x + pos.width;
  vert[3].pos.y = pos.y + pos.height;
  vert[4].pos.x = pos.x;
  vert[4].pos.y = pos.y + pos.height;
  vert[5].pos.x = pos.x;
  vert[5].pos.y = pos.y;

  vert[0].uv = base::Vec2(tex.x, tex.y);
  vert[1].uv = base::Vec2(tex.x + tex.width, tex.y);
  vert[2].uv = base::Vec2(tex.x + tex.width, tex.y + tex.height);
  vert[3].uv = base::Vec2(tex.x + tex.width, tex.y + tex.height);
  vert[4].uv = base::Vec2(tex.x, tex.y + tex.height);
  vert[5].uv = base::Vec2(tex.x, tex.y);

  vert += 6;
};

void CreateResources(RefCntAutoPtr<ISwapChain> swapchain,
                     RefCntAutoPtr<IRenderDevice> device,
                     RefCntAutoPtr<IPipelineState>& pso,
                     RefCntAutoPtr<IShaderResourceBinding>& srb,
                     RefCntAutoPtr<IBuffer>& ubo) {
  GraphicsPipelineStateCreateInfo PSOCreateInfo;
  PSOCreateInfo.PSODesc.Name = "Basic PSO";
  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

  PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
  PSOCreateInfo.GraphicsPipeline.RTVFormats[0] =
      swapchain->GetDesc().ColorBufferFormat;
  PSOCreateInfo.GraphicsPipeline.DSVFormat =
      swapchain->GetDesc().DepthBufferFormat;
  PSOCreateInfo.GraphicsPipeline.PrimitiveTopology =
      PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
  PSOCreateInfo.GraphicsPipeline.RasterizerDesc.ScissorEnable = True;
  PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

  ShaderCreateInfo ShaderCI;
  ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  ShaderCI.Desc.UseCombinedTextureSamplers = true;
  ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

  RefCntAutoPtr<IShader> pVS;
  {
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.EntryPoint = "main";
    ShaderCI.Desc.Name = "Triangle vertex shader";
    ShaderCI.Source = VSSource;
    device->CreateShader(ShaderCI, &pVS);

    CreateUniformBuffer(device, sizeof(CBConstants), "VS constants CB", &ubo);
  }

  RefCntAutoPtr<IShader> pPS;
  {
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.EntryPoint = "main";
    ShaderCI.Desc.Name = "Triangle pixel shader";
    ShaderCI.Source = PSSource;
    device->CreateShader(ShaderCI, &pPS);
  }

  PSOCreateInfo.pVS = pVS;
  PSOCreateInfo.pPS = pPS;

  LayoutElement LayoutElems[] = {LayoutElement{0, 0, 3, VT_FLOAT32, False},
                                 LayoutElement{1, 0, 2, VT_FLOAT32, False}};

  PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
  PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements =
      _countof(LayoutElems);

  ShaderResourceVariableDesc Vars[] = {
      {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
  PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
  PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_LINEAR,    FILTER_TYPE_LINEAR,
                                 FILTER_TYPE_LINEAR,    TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};
  ImmutableSamplerDesc ImtblSamplers[] = {
      {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};
  PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
  PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers =
      _countof(ImtblSamplers);

  device->CreateGraphicsPipelineState(PSOCreateInfo, &pso);
  pso->CreateShaderResourceBinding(&srb, true);
}

void CreateFramebuffer(RefCntAutoPtr<IRenderDevice> device,
                       RefCntAutoPtr<ISwapChain> swapchain,
                       RefCntAutoPtr<ITexture>& rt_tex) {
  TextureLoadInfo TexLoadInfo;
  TexLoadInfo.Name = "Offscreen render target";
  TexLoadInfo.MipLevels = 1;
  TexLoadInfo.Format = swapchain->GetCurrentBackBufferRTV()->GetDesc().Format;
  TexLoadInfo.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
  CreateTextureFromFile("bg.png", TexLoadInfo, device, &rt_tex);
}

void CreateVertexBuffer(RefCntAutoPtr<IRenderDevice> device,
                        RefCntAutoPtr<IBuffer>& vbo) {
  Vertex CubeVerts[6];
  set_rect(CubeVerts, {50, 50, 500, 500}, {0, 0, 500, 500});

  BufferDesc VertBuffDesc;
  VertBuffDesc.Name = "Quad vertex buffer";
  VertBuffDesc.Usage = USAGE_IMMUTABLE;
  VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
  VertBuffDesc.Size = sizeof(CubeVerts);

  BufferData VBData;
  VBData.pData = CubeVerts;
  VBData.DataSize = sizeof(CubeVerts);

  device->CreateBuffer(VertBuffDesc, &VBData, &vbo);
}

void CreateVertexBuffer2(RefCntAutoPtr<IRenderDevice> device,
                         RefCntAutoPtr<IBuffer>& vbo) {
  Vertex CubeVerts[6];
  set_rect(CubeVerts, {0, 0, 400, 300}, {0, 0, 800, 600});

  BufferDesc VertBuffDesc;
  VertBuffDesc.Name = "Screen quad vertex buffer";
  VertBuffDesc.Usage = USAGE_IMMUTABLE;
  VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
  VertBuffDesc.Size = sizeof(CubeVerts);

  BufferData VBData;
  VBData.pData = CubeVerts;
  VBData.DataSize = sizeof(CubeVerts);

  device->CreateBuffer(VertBuffDesc, &VBData, &vbo);
}

void ReadTexture(RefCntAutoPtr<IRenderDevice> device,
                 RefCntAutoPtr<IDeviceContext> context,
                 RefCntAutoPtr<ITexture> tex) {
  TextureDesc TexDesc = tex->GetDesc();
  TexDesc.Usage = USAGE_STAGING;
  TexDesc.BindFlags = BIND_NONE;
  TexDesc.CPUAccessFlags = CPU_ACCESS_READ;
  RefCntAutoPtr<ITexture> pStagingTex;
  device->CreateTexture(TexDesc, nullptr, &pStagingTex);

  context->SetRenderTargets(0, nullptr, nullptr,
                            RESOURCE_STATE_TRANSITION_MODE_NONE);

  CopyTextureAttribs CPTex(tex, RESOURCE_STATE_TRANSITION_MODE_NONE,
                           pStagingTex, RESOURCE_STATE_TRANSITION_MODE_NONE);
  context->CopyTexture(CPTex);
  context->WaitForIdle();

  MappedTextureSubresource MappedData;
  context->MapTextureSubresource(pStagingTex, 0, 0, MAP_READ,
                                 MAP_FLAG_DO_NOT_WAIT, nullptr, MappedData);

  int width = TexDesc.Width;
  int height = TexDesc.Height;
  int channels = 4;

  stbi_write_png("output.png", width, height, channels, MappedData.pData,
                 MappedData.Stride);

  context->UnmapTextureSubresource(pStagingTex, 0, 0);
}

void Render(RefCntAutoPtr<ISwapChain> swapchain,
            RefCntAutoPtr<ITexture> render_target,
            RefCntAutoPtr<IDeviceContext> context,
            RefCntAutoPtr<IPipelineState> pso,
            RefCntAutoPtr<IShaderResourceBinding> srb,
            RefCntAutoPtr<IBuffer> ubo,
            RefCntAutoPtr<IBuffer> vbo,
            RefCntAutoPtr<ITexture> texture,
            bool is_opengl) {
  auto* pRTV = swapchain
                   ? swapchain->GetCurrentBackBufferRTV()
                   : render_target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  context->SetRenderTargets(1, &pRTV, nullptr,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  if (!render_target) {
    const float ClearColor[] = {1, 0, 1, 1};
    context->ClearRenderTarget(pRTV, ClearColor,
                               RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }

  const float a =
      (swapchain ? swapchain->GetDesc().Width : render_target->GetDesc().Width);
  const float b = (swapchain ? swapchain->GetDesc().Height
                             : render_target->GetDesc().Height);
  {
    MapHelper<CBConstants> CBConstants(context, ubo, MAP_WRITE,
                                       MAP_FLAG_DISCARD);

    MakeProjectionMatrix(CBConstants->proj_mat, base::Vec2(a, b), is_opengl);
    CBConstants->tex_size = base::MakeInvert(
        base::Vec2(texture->GetDesc().Width, texture->GetDesc().Height));
  }

  const Uint64 offset = 0;
  IBuffer* pBuffs[] = {vbo};
  context->SetVertexBuffers(0, 1, pBuffs, &offset,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                            SET_VERTEX_BUFFERS_FLAG_RESET);
  context->SetPipelineState(pso);

  ITextureView* texview = texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
  srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(texview);
  srb->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(ubo);
  context->CommitShaderResources(srb,
                                 RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Rect scissor;
  scissor.right = 300;
  scissor.bottom = 300;
  context->SetScissorRects(1, &scissor, a, scissor.bottom + scissor.left);

  DrawAttribs drawAttrs;
  drawAttrs.NumVertices = 6;
  context->Draw(drawAttrs);
}

int native_test(int argc, char* argv[]);

int SDL_main(int argc, char* argv[]) {
  // return native_test(argc, argv);

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO);

  {
    std::unique_ptr<ui::Widget> widget(new ui::Widget);
    ui::Widget::InitParams win_params;
    win_params.size = base::Vec2i(800, 600);
    win_params.resizable = true;
    widget->Init(std::move(win_params));

    std::unique_ptr<renderer::RenderDevice> dev =
        renderer::RenderDevice::Create(
            widget->AsWeakPtr(),
            renderer::RenderDevice::RendererBackend::kD3D12);

    std::unique_ptr<renderer::QuadDrawable> quad(
        new renderer::QuadDrawable(dev->device(), dev->quad_index_buffer()));

    std::unique_ptr<renderer::QuadArray> quads(
        new renderer::QuadArray(dev->device(), dev->quad_index_buffer()));

    quads->Resize(3);

    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> tex, tex2;
    CreateTextureFromFile("test.png", loadInfo, dev->device(), &tex);
    CreateTextureFromFile("bg.png", loadInfo, dev->device(), &tex2);

    while (true) {
      SDL_Event e;
      SDL_PollEvent(&e);
      if (e.type == SDL_EVENT_QUIT)
        break;
      if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        int w, h;
        SDL_GetWindowSize(widget->AsSDLWindow(), &w, &h);
        dev->swapchain()->Resize(w, h);
      }

      auto* RTV = dev->swapchain()->GetCurrentBackBufferRTV();
      dev->context()->SetRenderTargets(
          1, &RTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      const float ClearColor[] = {1, 0, 1, 1};
      dev->context()->ClearRenderTarget(
          RTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      auto& shader = dev->GetPipelines()->base;
      auto* pipeline = shader.GetPSOFor(renderer::BlendType::Normal);

      dev->context()->SetPipelineState(pipeline->pso);

      {
        MapHelper<renderer::PipelineInstance_Base::UniformParams> CBConstants(
            dev->context(), shader.GetUniformBuffer(), MAP_WRITE,
            MAP_FLAG_DISCARD);
        MakeProjectionMatrix(CBConstants->projMat, widget->GetSize(),
                             dev->device()->GetDeviceInfo().IsGLDevice());
        CBConstants->texSize = base::MakeInvert(
            base::Vec2(tex->GetDesc().Width, tex->GetDesc().Height));
        CBConstants->transOffset = base::Vec2i(140, 40);
      }

      shader.SetTexture(tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
      dev->context()->CommitShaderResources(
          pipeline->srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      Rect scissor;
      scissor.left = 0;
      scissor.right = 800;
      scissor.top = 0;
      scissor.bottom = 600;
      dev->context()->SetScissorRects(1, &scissor, 1,
                                      scissor.bottom + scissor.left);

      quad->SetPosition({100, 100, 200, 200});
      quad->SetTexcoord({0, 0, 300, 300});
      quad->Draw(dev->context());

      quad->SetPosition({0, 0, 100, 100});
      quad->SetTexcoord({0, 0, 300, 300});
      quad->Draw(dev->context());

      shader.SetTexture(tex2->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
      dev->context()->CommitShaderResources(
          pipeline->srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      auto* vertices = quads->vertices().data();
      renderer::GeometryVertexLayout::SetTexPos(vertices, {0, 0, 100, 100},
                                                {300, 300, 100, 100});
      renderer::GeometryVertexLayout::SetTexPos(vertices + 4, {0, 0, 100, 100},
                                                {400, 300, 100, 100});
      renderer::GeometryVertexLayout::SetTexPos(vertices + 8, {0, 0, 100, 100},
                                                {300, 400, 100, 100});
      quads->Update(dev->context());
      quads->Draw(dev->context());

      dev->swapchain()->Present();
    }
  }

  SDL_Quit();

  return 0;
}

int native_test(int argc, char* argv[]) {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO);
  SDL_Window* win =
      SDL_CreateWindow("RGU Renderer Unittests", 800, 600,
                       SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_RESIZABLE);

  SDL_PropertiesID win_prop = SDL_GetWindowProperties(win);
  void* win_handle = SDL_GetPointerProperty(
      win_prop, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

  SwapChainDesc SCDesc;

  RefCntAutoPtr<IRenderDevice> device;
  RefCntAutoPtr<IDeviceContext> context;
  RefCntAutoPtr<ISwapChain> swapchain;

  bool use_d3d = true;
  for (int i = 0; i < argc; ++i) {
    if (std::string(argv[i]) == "opengl")
      use_d3d = false;
  }

  if (!use_d3d) {
    auto* factory = GetEngineFactoryOpenGL();
    EngineGLCreateInfo EngineCI;
    EngineCI.Window.hWnd = win_handle;
    factory->CreateDeviceAndSwapChainGL(EngineCI, &device, &context, SCDesc,
                                        &swapchain);
  } else {
    EngineD3D12CreateInfo EngineCI;
    auto* pFactoryD3D12 = GetEngineFactoryD3D12();
    pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &device, &context);
    Win32NativeWindow Window{win_handle};
    pFactoryD3D12->CreateSwapChainD3D12(
        device, context, SCDesc, FullScreenModeDesc{}, Window, &swapchain);
  }

  RefCntAutoPtr<IPipelineState> pso;
  RefCntAutoPtr<IShaderResourceBinding> srb;
  RefCntAutoPtr<IBuffer> ubo;
  CreateResources(swapchain, device, pso, srb, ubo);

  RefCntAutoPtr<IBuffer> vbo;
  CreateVertexBuffer(device, vbo);

  RefCntAutoPtr<IBuffer> vbo2;
  CreateVertexBuffer2(device, vbo2);

  TextureLoadInfo loadInfo;
  loadInfo.IsSRGB = true;
  RefCntAutoPtr<ITexture> tex;
  CreateTextureFromFile("test.png", loadInfo, device, &tex);
  RefCntAutoPtr<ITexture> tex2;
  CreateTextureFromFile("sam.png", loadInfo, device, &tex2);

  RefCntAutoPtr<ITexture> render_target;
  CreateFramebuffer(device, swapchain, render_target);

  ITextureView* rtview =
      render_target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

  bool readed = false;
  while (true) {
    SDL_Event e;
    SDL_PollEvent(&e);
    if (e.type == SDL_EVENT_QUIT)
      break;
    if (e.type == SDL_EVENT_WINDOW_RESIZED) {
      int w, h;
      SDL_GetWindowSize(win, &w, &h);
      swapchain->Resize(w, h);
    }

    Render(RefCntAutoPtr<ISwapChain>(), render_target, context, pso, srb, ubo,
           vbo, tex, !use_d3d);

    {
      CopyTextureAttribs CPAttr(tex2, RESOURCE_STATE_TRANSITION_MODE_NONE,
                                render_target,
                                RESOURCE_STATE_TRANSITION_MODE_NONE);
      context->SetRenderTargets(0, nullptr, nullptr,
                                RESOURCE_STATE_TRANSITION_MODE_NONE);
      context->CopyTexture(CPAttr);
    }

    Render(swapchain, RefCntAutoPtr<ITexture>(), context, pso, srb, ubo, vbo2,
           render_target, !use_d3d);

    swapchain->Present();

    if (!readed) {
      ReadTexture(device, context, render_target);
      readed = true;
    }
  }

  SDL_DestroyWindow(win);
  SDL_Quit();

  return 0;
}
