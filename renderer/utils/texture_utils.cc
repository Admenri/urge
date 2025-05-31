// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/utils/texture_utils.h"

namespace renderer {

void CreateTexture2D(Diligent::IRenderDevice* device,
                     Diligent::ITexture** texture,
                     const base::String& name,
                     const base::Vec2i& size,
                     Diligent::USAGE usage,
                     Diligent::BIND_FLAGS bind_flags,
                     Diligent::CPU_ACCESS_FLAGS access,
                     Diligent::TEXTURE_FORMAT format) {
  Diligent::TextureDesc texture_desc;
  texture_desc.Name = name.c_str();
  texture_desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
  texture_desc.Width = size.x;
  texture_desc.Height = size.y;
  texture_desc.Format = format;
  texture_desc.BindFlags = bind_flags;
  texture_desc.Usage = usage;
  texture_desc.CPUAccessFlags = access;

  device->CreateTexture(texture_desc, nullptr, texture);
}

void CreateTexture2D(Diligent::IRenderDevice* device,
                     Diligent::ITexture** texture,
                     const base::String& name,
                     SDL_Surface* data,
                     Diligent::USAGE usage,
                     Diligent::BIND_FLAGS bind_flags,
                     Diligent::CPU_ACCESS_FLAGS access,
                     Diligent::TEXTURE_FORMAT format) {
  Diligent::TextureDesc texture_desc;
  texture_desc.Name = name.c_str();
  texture_desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
  texture_desc.Width = data->w;
  texture_desc.Height = data->h;
  texture_desc.Format = format;
  texture_desc.BindFlags = bind_flags;
  texture_desc.Usage = usage;
  texture_desc.CPUAccessFlags = access;

  Diligent::TextureSubResData texture_sub_res_data;
  texture_sub_res_data.pData = data->pixels;
  texture_sub_res_data.Stride = data->pitch;

  Diligent::TextureData texture_data;
  texture_data.pSubResources = &texture_sub_res_data;
  texture_data.NumSubresources = 1;

  device->CreateTexture(texture_desc, &texture_data, texture);
}

}  // namespace renderer