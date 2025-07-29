// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_UTILS_TEXTURE_UTILS_H_
#define RENDERER_UTILS_TEXTURE_UTILS_H_

#include "SDL3/SDL_surface.h"

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"

#include "base/math/rectangle.h"

namespace renderer {

void CreateTexture2D(
    Diligent::IRenderDevice* device,
    Diligent::ITexture** texture,
    const std::string& name,
    const base::Vec2i& size,
    Diligent::USAGE usage = Diligent::USAGE_DEFAULT,
    Diligent::BIND_FLAGS bind_flags = Diligent::BIND_SHADER_RESOURCE,
    Diligent::CPU_ACCESS_FLAGS access = Diligent::CPU_ACCESS_NONE,
    Diligent::TEXTURE_FORMAT format =
        Diligent::TEXTURE_FORMAT::TEX_FORMAT_RGBA8_UNORM);

void CreateTexture2D(
    Diligent::IRenderDevice* device,
    Diligent::ITexture** texture,
    const std::string& name,
    SDL_Surface* data,
    Diligent::USAGE usage = Diligent::USAGE_DEFAULT,
    Diligent::BIND_FLAGS bind_flags = Diligent::BIND_SHADER_RESOURCE,
    Diligent::CPU_ACCESS_FLAGS access = Diligent::CPU_ACCESS_NONE,
    Diligent::TEXTURE_FORMAT format =
        Diligent::TEXTURE_FORMAT::TEX_FORMAT_RGBA8_UNORM);

}  // namespace renderer

#endif  //! RENDERER_UTILS_TEXTURE_UTILS_H_
