// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/tilemap2.h"

#include "content/public/bitmap.h"

#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_surface.h"

namespace {

#define OVER_PLAYER_FLAG (1 << 4)
#define TABLE_FLAG (1 << 7)

static const int kGroundLayerDefaultZ = 0;
static const int kAboveLayerDefaultZ = 200;

using AutotileSubPos = enum {
  TopLeft = 0,
  TopRight,
  BottomLeft,
  BottomRight,
  BottomLeftTable,
  BottomRightTable,
};

using TilemapBlock = struct {
  base::Vec2i pos;
  base::Vec2i size;
};

using TilemapVXAtlasBlock = struct {
  content::Tilemap2::TilemapBitmapID tile_id;
  TilemapBlock src;
  base::Vec2i dst;
};

const uint8_t kAniIndicesRegular[3 * 4] = {0, 1, 2, 1, 0, 1, 2, 1, 0, 1, 2, 1};
const uint8_t kAniIndicesWaterfall[3 * 4] = {0, 1, 2, 0, 1, 2,
                                             0, 1, 2, 0, 1, 2};

const uint8_t kFlashAlpha[] = {
    /* Fade in */
    0x78, 0x78, 0x78, 0x78, 0x96, 0x96, 0x96, 0x96, 0xB4, 0xB4, 0xB4, 0xB4,
    0xD2, 0xD2, 0xD2, 0xD2,
    /* Fade out */
    0xF0, 0xF0, 0xF0, 0xF0, 0xD2, 0xD2, 0xD2, 0xD2, 0xB4, 0xB4, 0xB4, 0xB4,
    0x96, 0x96, 0x96, 0x96};

const int kFlashAlphaSize = sizeof(kFlashAlpha) / sizeof(kFlashAlpha[0]);

const base::RectF kAutotileSrcRegular[] = {
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
};

const int kAutotileSrcRegularSize =
    sizeof(kAutotileSrcRegular) / sizeof(kAutotileSrcRegular[0]);

const base::RectF kAutotileSrcTable[] = {
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
};

const base::RectF kAutotileSrcWall[] = {
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
};

const int kAutotileSrcWallSize =
    sizeof(kAutotileSrcWall) / sizeof(kAutotileSrcWall[0]);

const base::RectF kAutotileSrcWaterfall[] = {
    {1.0f, 0.0f, 0.5f, 1.0f}, {0.5f, 0.0f, 0.5f, 1.0f},
    {0.0f, 0.0f, 0.5f, 1.0f}, {0.5f, 0.0f, 0.5f, 1.0f},
    {1.0f, 0.0f, 0.5f, 1.0f}, {1.5f, 0.0f, 0.5f, 1.0f},
    {0.0f, 0.0f, 0.5f, 1.0f}, {1.5f, 0.0f, 0.5f, 1.0f},
};

const TilemapVXAtlasBlock kTilemapAtlas[] = {
    /* A1 tilemap */
    {
        content::Tilemap2::TileA1,
        {
            base::Vec2i{0, 0},
            base::Vec2i{6, 6},
        },
        base::Vec2i{0, 0},
    },
    {
        content::Tilemap2::TileA1,
        {
            base::Vec2i{8, 0},
            base::Vec2i{6, 6},
        },
        base::Vec2i{6, 0},
    },
    {
        content::Tilemap2::TileA1,
        {
            base::Vec2i{0, 6},
            base::Vec2i{6, 6},
        },
        base::Vec2i{0, 6},
    },
    {
        content::Tilemap2::TileA1,
        {
            base::Vec2i{8, 6},
            base::Vec2i{6, 6},
        },
        base::Vec2i{6, 6},
    },
    {
        content::Tilemap2::TileA1,
        {
            base::Vec2i{6, 0},
            base::Vec2i{2, 12},
        },
        base::Vec2i{12, 0},
    },
    {
        content::Tilemap2::TileA1,
        {
            base::Vec2i{14, 0},
            base::Vec2i{2, 12},
        },
        base::Vec2i{14, 0},
    },
    /* A2 tilemap */
    {
        content::Tilemap2::TileA2,
        {
            base::Vec2i{0, 0},
            base::Vec2i{16, 12},
        },
        base::Vec2i{16, 0},
    },
    /* A3 tilemap */
    {
        content::Tilemap2::TileA3,
        {
            base::Vec2i{0, 0},
            base::Vec2i{16, 8},
        },
        base::Vec2i{0, 12},
    },
    /* A4 tilemap */
    {
        content::Tilemap2::TileA4,
        {
            base::Vec2i{0, 0},
            base::Vec2i{16, 15},
        },
        base::Vec2i{16, 12},
    },
    /* A5 tilemap */
    {
        content::Tilemap2::TileA5,
        {
            base::Vec2i{0, 0},
            base::Vec2i{8, 8},
        },
        base::Vec2i{0, 20},
    },
    {
        content::Tilemap2::TileA5,
        {
            base::Vec2i{0, 8},
            base::Vec2i{8, 8},
        },
        base::Vec2i{8, 20},
    },
    /* B tilemap */
    {
        content::Tilemap2::TileB,
        {
            base::Vec2i{0, 0},
            base::Vec2i{16, 16},
        },
        base::Vec2i{32, 0},
    },
    /* C tilemap */
    {
        content::Tilemap2::TileC,
        {
            base::Vec2i{0, 0},
            base::Vec2i{16, 16},
        },
        base::Vec2i{48, 0},
    },
    /* D tilemap */
    {
        content::Tilemap2::TileD,
        {
            base::Vec2i{0, 0},
            base::Vec2i{16, 16},
        },
        base::Vec2i{32, 16},
    },
    /* E tilemap */
    {
        content::Tilemap2::TileE,
        {
            base::Vec2i{0, 0},
            base::Vec2i{16, 16},
        },
        base::Vec2i{48, 16},
    },
};

const size_t kTilemapAtlasSize =
    sizeof(kTilemapAtlas) / sizeof(kTilemapAtlas[0]);

const base::RectF kShadowAtlasArea = {16.0f, 27.0f, 16.0f, 1.0f};

const base::RectF kFreeAtlasArea = {0.0f, 28.0f, 16.0f, 4.0f};

SDL_Surface* CreateShadowSet(int tilesize) {
  SDL_Surface* surf = SDL_CreateSurface(kShadowAtlasArea.width * tilesize,
                                        kShadowAtlasArea.height * tilesize,
                                        SDL_PIXELFORMAT_ABGR8888);

  std::vector<SDL_Rect> rects;
  SDL_Rect rect = {0, 0, tilesize / 2, tilesize / 2};

  for (int val = 0; val < 16; ++val) {
    int offsetX = val * tilesize;

    /* Top left */
    if (val & (1 << 0)) {
      rect.x = offsetX;
      rect.y = 0;
      rects.push_back(rect);
    }

    /* Top Right */
    if (val & (1 << 1)) {
      rect.x = offsetX + tilesize / 2;
      rect.y = 0;
      rects.push_back(rect);
    }

    /* Bottom left */
    if (val & (1 << 2)) {
      rect.x = offsetX;
      rect.y = tilesize / 2;
      rects.push_back(rect);
    }

    /* Bottom right */
    if (val & (1 << 3)) {
      rect.x = offsetX + tilesize / 2;
      rect.y = tilesize / 2;
      rects.push_back(rect);
    }
  }

  /* Fill rects with half opacity black */
  auto* pixel_detail = SDL_GetPixelFormatDetails(surf->format);
  uint32_t color = (0x80808080 & pixel_detail->Amask);
  SDL_FillSurfaceRects(surf, rects.data(), rects.size(), color);

  return surf;
}

}  // namespace

namespace content {

class TilemapGroundLayer2 : public ViewportChild {
 public:
  TilemapGroundLayer2(scoped_refptr<Graphics> screen,
                      base::WeakPtr<Tilemap2> tilemap)
      : ViewportChild(screen, tilemap->viewport_, kGroundLayerDefaultZ),
        tilemap_(tilemap) {}

  TilemapGroundLayer2(const TilemapGroundLayer2&) = delete;
  TilemapGroundLayer2& operator=(const TilemapGroundLayer2&) = delete;

  void PrepareDraw() override { tilemap_->BeforeTilemapComposite(); }
  void OnDraw(CompositeTargetInfo* target_info) override {
    tilemap_->DrawGroundLayerInternal(target_info);
  }

  void CheckObjectDisposed() const override { tilemap_->CheckIsDisposed(); }
  void OnParentViewportRectChanged(
      const DrawableParent::ViewportInfo& rect) override {
    tilemap_->buffer_need_update_ = true;
  }

  base::WeakPtr<Tilemap2> tilemap_;
};

class TilemapAboveLayer2 : public ViewportChild {
 public:
  TilemapAboveLayer2(scoped_refptr<Graphics> screen,
                     base::WeakPtr<Tilemap2> tilemap)
      : ViewportChild(screen, tilemap->viewport_, kAboveLayerDefaultZ),
        tilemap_(tilemap) {}

  TilemapAboveLayer2(const TilemapAboveLayer2&) = delete;
  TilemapAboveLayer2& operator=(const TilemapAboveLayer2&) = delete;

  void PrepareDraw() override { tilemap_->BeforeTilemapComposite(); }
  void OnDraw(CompositeTargetInfo* target_info) override {
    tilemap_->DrawAboveLayerInternal(target_info);
  }

  void CheckObjectDisposed() const override { tilemap_->CheckIsDisposed(); }
  void OnParentViewportRectChanged(
      const DrawableParent::ViewportInfo&) override {
    tilemap_->buffer_need_update_ = true;
  }

  base::WeakPtr<Tilemap2> tilemap_;
};

Tilemap2::Tilemap2(scoped_refptr<Graphics> screen,
                   scoped_refptr<Viewport> viewport,
                   int tilesize)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      viewport_(viewport),
      tile_size_(tilesize) {
  InitDrawableData();

  ground_ = std::make_unique<TilemapGroundLayer2>(
      screen, weak_ptr_factory_.GetWeakPtr());
  above_ = std::make_unique<TilemapAboveLayer2>(screen,
                                                weak_ptr_factory_.GetWeakPtr());
}

Tilemap2::~Tilemap2() {
  Dispose();
}

void Tilemap2::Update() {
  CheckIsDisposed();

  if (++frame_index_ >= 30 * 3 * 4)
    frame_index_ = 0;

  uint8_t aniIdxA = kAniIndicesRegular[frame_index_ / 30];
  uint8_t aniIdxC = kAniIndicesWaterfall[frame_index_ / 30];
  animation_offset_ =
      base::Vec2(aniIdxA * 2 * tile_size_, aniIdxC * tile_size_);

  if (++flash_alpha_index_ >= kFlashAlphaSize)
    flash_alpha_index_ = 0;
}

scoped_refptr<Bitmap> Tilemap2::GetBitmap(int index) const {
  CheckIsDisposed();
  return bitmaps_[index];
}

void Tilemap2::SetBitmap(int index, scoped_refptr<Bitmap> bitmap) {
  CheckIsDisposed();

  if (bitmaps_[index] == bitmap)
    return;

  bitmaps_[index] = bitmap;
  atlas_need_update_ = true;

  if (IsObjectValid(bitmap.get()))
    bitmap_observers_[index] = bitmap->AddBitmapObserver(base::BindRepeating(
        &Tilemap2::SetAtlasUpdateInternal, base::Unretained(this)));
}

scoped_refptr<Table> Tilemap2::GetMapData() const {
  CheckIsDisposed();
  return map_data_;
}

void Tilemap2::SetMapData(scoped_refptr<Table> map_data) {
  CheckIsDisposed();

  if (map_data_ == map_data)
    return;

  map_data_ = map_data;
  buffer_need_update_ = true;

  if (map_data_)
    map_data_observer_ = map_data_->AddObserver(base::BindRepeating(
        &Tilemap2::UpdateMapBufferInternal, base::Unretained(this)));
}

scoped_refptr<Table> Tilemap2::GetFlashData() const {
  CheckIsDisposed();
  return flash_data_;
}

void Tilemap2::SetFlashData(scoped_refptr<Table> flash_data) {
  CheckIsDisposed();

  if (flash_data_ == flash_data)
    return;
  flash_data_ = flash_data;
  flash_layer_->SetFlashData(flash_data_);
}

scoped_refptr<Table> Tilemap2::GetFlags() const {
  CheckIsDisposed();
  return flags_;
}

void Tilemap2::SetFlags(scoped_refptr<Table> flags) {
  CheckIsDisposed();

  if (flags_ == flags)
    return;

  flags_ = flags;
  buffer_need_update_ = true;

  if (flags_)
    flags_observer_ = flags_->AddObserver(base::BindRepeating(
        &Tilemap2::UpdateMapBufferInternal, base::Unretained(this)));
}

scoped_refptr<Viewport> Tilemap2::GetViewport() const {
  CheckIsDisposed();
  return viewport_;
}

void Tilemap2::SetViewport(scoped_refptr<Viewport> viewport) {
  CheckIsDisposed();

  if (!IsObjectValid(viewport.get()))
    return;

  if (viewport_ == viewport)
    return;

  viewport_ = viewport;
  ground_->SetViewport(viewport_);
  above_->SetViewport(viewport_);
  above_->SetZ(kAboveLayerDefaultZ);
}

bool Tilemap2::GetVisible() const {
  CheckIsDisposed();
  return visible_;
}

void Tilemap2::SetVisible(bool visible) {
  CheckIsDisposed();

  ground_->SetVisible(visible);
  above_->SetVisible(visible);
}

int Tilemap2::GetOX() const {
  CheckIsDisposed();
  return origin_.x;
}

void Tilemap2::SetOX(int ox) {
  CheckIsDisposed();
  origin_.x = ox;
}

int Tilemap2::GetOY() const {
  CheckIsDisposed();
  return origin_.y;
}

void Tilemap2::SetOY(int oy) {
  CheckIsDisposed();
  origin_.y = oy;
}

void Tilemap2::OnObjectDisposed() {
  ground_.reset();
  above_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();

  tilemap_quads_.reset();
  flash_layer_.reset();
  atlas_texture_.Release();
}

void Tilemap2::BeforeTilemapComposite() {
  flash_layer_->BeforeComposite(screen()->renderer()->context());

  UpdateTilemapViewportInternal();

  if (atlas_need_update_) {
    CreateTileAtlasInternal();
    atlas_need_update_ = false;
  }

  if (buffer_need_update_) {
    ParseMapDataBufferInternal();
    buffer_need_update_ = false;
  }
}

void Tilemap2::CreateTileAtlasInternal() {
  {
    Diligent::TextureDesc TexDesc;
    TexDesc.Name = "TilemapVX atlas texture";
    TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    TexDesc.Format = screen()->tex_format();
    TexDesc.Usage = Diligent::USAGE_DEFAULT;
    TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    TexDesc.BindFlags =
        Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
    TexDesc.Width = tile_size_ * 64;
    TexDesc.Height = tile_size_ * 32;

    atlas_texture_.Release();
    screen()->renderer()->device()->CreateTexture(TexDesc, nullptr,
                                                  &atlas_texture_);
  }

  auto* RTV =
      atlas_texture_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  screen()->renderer()->context()->SetRenderTargets(
      1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  float ClearColor[4] = {0, 0, 0, 0};
  screen()->renderer()->context()->ClearRenderTarget(
      RTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  screen()->renderer()->context()->SetRenderTargets(
      0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  /* tilemap bitmap atlas */
  for (size_t i = 0; i < kTilemapAtlasSize; ++i) {
    auto& atlas_info = kTilemapAtlas[i];
    scoped_refptr<Bitmap> atlas_bitmap = bitmaps_[atlas_info.tile_id];

    if (!IsObjectValid(atlas_bitmap.get()))
      continue;

    base::Rect src_rect(
        atlas_info.src.pos.x * tile_size_, atlas_info.src.pos.y * tile_size_,
        atlas_info.src.size.x * tile_size_, atlas_info.src.size.y * tile_size_);
    base::Rect dst_rect(
        atlas_info.dst.x * tile_size_, atlas_info.dst.y * tile_size_,
        atlas_info.src.size.x * tile_size_, atlas_info.src.size.y * tile_size_);

    renderer::CopyTexture(screen()->renderer()->context(),
                          atlas_bitmap->GetHandle(), src_rect, atlas_texture_,
                          dst_rect.Position());
  }

  /* shadow set atlas */
  if (screen()->api_version() >= APIVersion::RGSS3) {
    SDL_Surface* shadow_set = CreateShadowSet(tile_size_);

    Diligent::TextureSubResData TexSubData(shadow_set->pixels,
                                           shadow_set->pitch);
    Diligent::Box DstBox(
        kShadowAtlasArea.x * tile_size_,
        kShadowAtlasArea.width * tile_size_ + kShadowAtlasArea.x * tile_size_,
        kShadowAtlasArea.y * tile_size_,
        kShadowAtlasArea.height * tile_size_ + kShadowAtlasArea.y * tile_size_);
    screen()->renderer()->context()->UpdateTexture(
        atlas_texture_, 0, 0, DstBox, TexSubData,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    SDL_DestroySurface(shadow_set);
  }
}

void Tilemap2::UpdateTilemapViewportInternal() {
  auto& viewport_rect = ground_->parent_rect();

  const base::Vec2i tilemap_origin = origin_ + viewport_rect.origin;
  const base::Vec2i viewport_size = viewport_rect.rect.Size();

  base::Rect new_tilemap_viewport;
  new_tilemap_viewport.x = tilemap_origin.x / tile_size_;
  new_tilemap_viewport.y = tilemap_origin.y / tile_size_ - 1;
  new_tilemap_viewport.width =
      (viewport_size.x / tile_size_) + !!(viewport_size.x % tile_size_) + 1;
  new_tilemap_viewport.height =
      (viewport_size.y / tile_size_) + !!(viewport_size.y % tile_size_) + 2;
  if (!(new_tilemap_viewport == tilemap_viewport_)) {
    tilemap_viewport_ = new_tilemap_viewport;
    buffer_need_update_ = true;

    flash_layer_->SetViewport(tilemap_viewport_);
  }

  const base::Vec2i display_offset =
      tilemap_origin % base::Vec2i(tile_size_, tile_size_);
  tilemap_offset_ = viewport_rect.rect.Position() - display_offset -
                    base::Vec2i(0, tile_size_);
}

void Tilemap2::ParseMapDataBufferInternal() {
  ground_vertices_.clear();
  above_vertices_.clear();

  scoped_refptr<Table> mapdata = map_data_;
  scoped_refptr<Table> flagdata = flags_;

  auto alloc_ground = [&](int n) {
    size_t vert_size = ground_vertices_.size();
    ground_vertices_.resize(vert_size + n * 4);
    return &ground_vertices_[vert_size];
  };

  auto alloc_above = [&](int n) {
    size_t vert_size = above_vertices_.size();
    above_vertices_.resize(vert_size + n * 4);
    return &above_vertices_[vert_size];
  };

  auto process_quad = [&](base::RectF* texcoords, base::RectF* position,
                          int size, bool above) {
    auto* vert = above ? alloc_above(size) : alloc_ground(size);

    for (int i = 0; i < size; ++i)
      renderer::GeometryVertexLayout::SetTexPos(vert + i * 4, texcoords[i],
                                                position[i]);
  };

  auto autotile_subpos = [&](base::RectF& pos, int i) {
    switch (i) {
      case TopLeft:
        break;
      case TopRight:
        pos.x += tile_size_ / 2.0f;
        break;
      case BottomLeft:
        pos.y += tile_size_ / 2.0f;
        break;
      case BottomRight:
        pos.x += tile_size_ / 2.0f;
        pos.y += tile_size_ / 2.0f;
        break;
      case BottomLeftTable:
        pos.y += tile_size_ * 0.75f;
        break;
      case BottomRightTable:
        pos.x += tile_size_ / 2.0f;
        pos.y += tile_size_ * 0.75f;
        break;
      default:
        break;
    }
  };

  auto read_autotile_common = [&](int patternID, const base::Vec2i& offset,
                                  int x, int y, const base::RectF rect_src[],
                                  int rect_src_size, bool above) {
    base::RectF tex[4], pos[4];
    for (int i = 0; i < 4; ++i) {
      base::RectF tex_rect =
          rect_src[patternID * 4 + i] *
          base::RectF(tile_size_, tile_size_, tile_size_, tile_size_);
      tex_rect.x += 0.5f;
      tex_rect.y += 0.5f;
      tex_rect.width -= 1;
      tex_rect.height -= 1;

      tex[i] = tex_rect;
      tex[i].x += offset.x * tile_size_;
      tex[i].y += offset.y * tile_size_;

      pos[i] = base::RectF(x * tile_size_, y * tile_size_, tile_size_ / 2.0f,
                           tile_size_ / 2.0f);
      autotile_subpos(pos[i], i);
    }

    process_quad(tex, pos, 4, above);
  };

  auto read_autotile_table = [&](int patternID, const base::Vec2i& offset,
                                 int x, int y, bool occlusion, bool above) {
    base::RectF tex[6], pos[6];
    for (int i = 0; i < 6; ++i) {
      base::RectF tex_rect =
          kAutotileSrcTable[patternID * 6 + i] *
          base::RectF(tile_size_, tile_size_, tile_size_, tile_size_);
      tex_rect.x += 0.5f;
      tex_rect.y += 0.5f;
      tex_rect.width -= 1;
      tex_rect.height -= 1;

      tex_rect.width = std::max(0.0f, tex_rect.width);
      tex_rect.height = std::max(0.0f, tex_rect.height);

      tex[i] = tex_rect;
      tex[i].x += offset.x * tile_size_;
      tex[i].y += offset.y * tile_size_;

      auto tex_size = kAutotileSrcTable[patternID * 6 + i].Size() *
                      base::Vec2(tile_size_, tile_size_);
      pos[i] =
          base::RectF(x * tile_size_, y * tile_size_, tex_size.x, tex_size.y);
      autotile_subpos(pos[i], i);

      if (occlusion && i >= 4) {
        float table_leg = tile_size_ * 0.25f;
        tex[i].height -= table_leg;
        pos[i].height -= table_leg;
      }
    }

    process_quad(tex, pos, 6, above);
  };

  auto read_autotile_waterfall = [&](int patternID, const base::Vec2i& offset,
                                     int x, int y, bool above) {
    if (patternID > 0x3)
      return;

    base::RectF tex[2], pos[2];
    for (size_t i = 0; i < 2; ++i) {
      base::RectF tex_rect =
          kAutotileSrcWaterfall[patternID * 2 + i] *
          base::RectF(tile_size_, tile_size_, tile_size_, tile_size_);
      tex_rect.x += 0.5f;
      tex_rect.y += 0.5f;
      tex_rect.width -= 1;
      tex_rect.height -= 1;

      tex[i] = tex_rect;
      tex[i].x += offset.x * tile_size_;
      tex[i].y += offset.y * tile_size_;

      pos[i] = base::RectF(x * tile_size_, y * tile_size_, tile_size_ / 2.0f,
                           tile_size_);
      pos[i].x += i * (tile_size_ / 2.0f);
    }

    process_quad(tex, pos, 2, above);
  };

  auto process_tile_A1 = [&](int16_t tileID, int x, int y, bool above) {
    tileID -= 0x0800;

    int patternID = tileID % 0x30;
    int autotileID = tileID / 0x30;

    // clang-format off
    const base::Vec2i waterfall(-1, -1);
    const base::Vec2i src_offset[] = {
        {0,  0},  {0,  3},
        {12, 0},  {12, 3},
        {6,  0},  waterfall,
        {6,  3},  waterfall,

        {0,  6},  waterfall,
        {0,  9},  waterfall,
        {6,  6},  waterfall,
        {6,  9},  waterfall};
    const base::Vec2i waterfall_offset[] = {
        {14, 0}, {14, 3},
        {12, 6}, {12, 9},
        {14, 6}, {14, 9},
    };
    // clang-format on

    const base::Vec2i src_pos = src_offset[autotileID];
    if (src_pos.x == -1) {
      int cID = (autotileID - 5) / 2;
      const base::Vec2i orig = waterfall_offset[cID];
      return read_autotile_waterfall(patternID, orig, x, y, above);
    }

    read_autotile_common(patternID, src_pos, x, y, kAutotileSrcRegular,
                         kAutotileSrcRegularSize, above);
  };

  auto process_tile_A2 = [&](int16_t tileID, int x, int y, bool above,
                             bool is_table, bool occlusion) {
    base::Vec2i offset(16, 0);
    tileID -= 0x0B00;

    int patternID = tileID % 0x30;
    int autotileID = tileID / 0x30;

    offset.x += (autotileID % 8) * 2;
    offset.y += (autotileID / 8) * 3;

    if (is_table)
      return read_autotile_table(patternID, offset, x, y, occlusion, above);

    read_autotile_common(patternID, offset, x, y, kAutotileSrcRegular,
                         kAutotileSrcRegularSize, above);
  };

  auto process_tile_A3 = [&](int16_t tileID, int x, int y, bool above) {
    base::Vec2i offset(0, 12);
    tileID -= 0x1100;

    int patternID = tileID % 0x30;
    int autotileID = tileID / 0x30;

    offset.x += (autotileID % 8) * 2;
    offset.y += (autotileID / 8) * 2;

    if (patternID >= 0x10)
      return;

    read_autotile_common(patternID, offset, x, y, kAutotileSrcWall,
                         kAutotileSrcWallSize, above);
  };

  auto process_tile_A4 = [&](int16_t tileID, int x, int y, bool above) {
    base::Vec2i offset(16, 12);
    tileID -= 0x1700;

    static const int offY[] = {0, 3, 5, 8, 10, 13};
    int patternID = tileID % 0x30;
    int autotileID = tileID / 0x30;

    int offYI = autotileID / 8;
    offset.x += (autotileID % 8) * 2;
    offset.y += offY[offYI];

    if ((offYI % 2) == 0) {
      read_autotile_common(patternID, offset, x, y, kAutotileSrcRegular,
                           kAutotileSrcRegularSize, above);
    } else {
      if (patternID >= 0x10)
        return;

      read_autotile_common(patternID, offset, x, y, kAutotileSrcWall,
                           kAutotileSrcWallSize, above);
    }
  };

  auto process_tile_A5 = [&](int16_t tileID, int x, int y, bool above) {
    const base::Vec2i src_origin(0, 20);

    tileID -= 0x0600;
    int ox = tileID % 0x8;
    int oy = tileID / 0x8;

    /* A5 half */
    if (oy >= 8) {
      oy -= 8;
      ox += 8;
    }

    base::RectF tex((src_origin.x + ox) * tile_size_ + 0.5f,
                    (src_origin.y + oy) * tile_size_ + 0.5f, tile_size_ - 1.0f,
                    tile_size_ - 1.0f);
    base::RectF pos(x * tile_size_, y * tile_size_, tile_size_, tile_size_);

    process_quad(&tex, &pos, 1, above);
  };

  auto process_tile_bcde = [&](int16_t tileID, int x, int y, bool above) {
    int ox = tileID % 0x8;
    int oy = (tileID / 0x8) % 0x10;
    int ob = tileID / (0x8 * 0x10);

    ox += (ob % 2) * 0x8;
    oy += (ob / 2) * 0x10;

    if (oy >= 48) {
      /* E atlas */
      oy -= 32;
      ox += 16;
    } else if (oy >= 32) {
      /* D atlas */
      oy -= 16;
    } else if (oy >= 16) {
      /* C atlas */
      oy -= 16;
      ox += 16;
    }

    base::RectF tex((32 + ox) * tile_size_ + 0.5f, (0 + oy) * tile_size_ + 0.5f,
                    tile_size_ - 1.0f, tile_size_ - 1.0f);
    base::RectF pos(x * tile_size_, y * tile_size_, tile_size_, tile_size_);

    process_quad(&tex, &pos, 1, above);
  };

  auto each_tile = [&](int16_t tileID, int x, int y, int z,
                       int16_t underTileID) {
    int16_t flag = TableGetFlag(flagdata, tileID);
    bool over_player = (flag & OVER_PLAYER_FLAG) && (z >= 2);
    bool is_table;

    if (screen()->api_version() >= APIVersion::RGSS3)
      is_table = flag & TABLE_FLAG;
    else
      is_table = (tileID - 0x0B00) % (8 * 0x30) >= (7 * 0x30);

    /* B ~ E */
    if (tileID < 0x0400)
      return process_tile_bcde(tileID, x, y, over_player);
    /* A5 */
    if (tileID >= 0x0600 && tileID < 0x0680)
      return process_tile_A5(tileID, x, y, over_player);
    /* A1 */
    if (tileID >= 0x0800 && tileID < 0x0B00)
      return process_tile_A1(tileID, x, y, over_player);
    /* A2 */
    if (tileID >= 0x0B00 && tileID < 0x1100)
      return process_tile_A2(tileID, x, y, over_player, is_table,
                             underTileID >= 0x1700 && underTileID < 0x2000);
    /* A3 */
    if (tileID < 0x1700)
      return process_tile_A3(tileID, x, y, over_player);
    /* A4 */
    if (tileID < 0x2000)
      return process_tile_A4(tileID, x, y, over_player);
  };

  auto shadow_tile = [&](int8_t shadow_id, int x, int y) {
    int ox = shadow_id;

    base::RectF tex((kShadowAtlasArea.x + ox) * tile_size_ + 0.5f,
                    (kShadowAtlasArea.y) * tile_size_ + 0.5f, tile_size_ - 1.0f,
                    tile_size_ - 1.0f);
    base::RectF pos(x * tile_size_, y * tile_size_, tile_size_, tile_size_);

    process_quad(&tex, &pos, 1, false);
  };

  auto process_shadow_layer = [&](int ox, int oy, int w, int h) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        int16_t value = TableGetWrapped(mapdata, x + ox, y + oy, 3);
        shadow_tile(value & 0xF, x, y);
      }
    }
  };

  auto process_layer = [&](int ox, int oy, int w, int h, int z) {
    for (int y = h - 1; y >= 0; --y) {
      for (int x = 0; x < w; ++x) {
        if (z <= 2) {
          int16_t tileID = TableGetWrapped(mapdata, x + ox, y + oy, z);
          int16_t underTileID = TableGetWrapped(mapdata, x + ox, y + oy + 1, 0);

          if (tileID > 0)
            each_tile(tileID, x, y, z, underTileID);
        } else {
          int16_t shadowID = TableGetWrapped(mapdata, x + ox, y + oy, 3);

          if (shadowID > 0)
            shadow_tile(shadowID & 0xF, x, y);
        }
      }
    }
  };

  auto read_tilemap = [&](const base::Rect& viewport) {
    int ox = viewport.x, oy = viewport.y;
    int w = viewport.width, h = viewport.height;

    /* autotile A area */
    process_layer(ox, oy, w, h, 0);

    process_layer(ox, oy, w, h, 1);

    /* shadow layer */
    if (screen()->api_version() >= APIVersion::RGSS3)
      process_shadow_layer(ox, oy, w, h);

    /* BCDE area */
    process_layer(ox, oy, w, h, 2);
  };

  /* Process tilemap data */
  read_tilemap(tilemap_viewport_);

  /* Process quad array */
  size_t quad_size = (ground_vertices_.size() + above_vertices_.size()) / 4;
  tilemap_quads_->Resize(screen()->renderer()->context(), quad_size);

  auto* base_addr = tilemap_quads_->vertices().data();

  if (!ground_vertices_.empty()) {
    memcpy(
        base_addr, ground_vertices_.data(),
        ground_vertices_.size() * sizeof(renderer::GeometryVertexLayout::Data));
  }

  if (!above_vertices_.empty()) {
    memcpy(
        base_addr + ground_vertices_.size(), above_vertices_.data(),
        above_vertices_.size() * sizeof(renderer::GeometryVertexLayout::Data));
  }
}

void Tilemap2::SetAtlasUpdateInternal() {
  atlas_need_update_ = true;
}

void Tilemap2::UpdateMapBufferInternal() {
  buffer_need_update_ = true;
}

void Tilemap2::InitDrawableData() {
  if (flash_layer_)
    return;

  flash_layer_.reset(
      new TilemapFlashLayer(tile_size_, screen()->renderer()->device(),
                            screen()->renderer()->quad_index_buffer()));
  tilemap_quads_.reset(
      new renderer::QuadArray(screen()->renderer()->device(),
                              screen()->renderer()->quad_index_buffer()));
}

void Tilemap2::DrawFlashLayerInternal(CompositeTargetInfo* target_info) {
  float alpha = (kFlashAlpha[flash_alpha_index_] / 255.0f) / 2;
  flash_layer_->Composite(target_info, screen(), tilemap_offset_, alpha);
}

void Tilemap2::DrawGroundLayerInternal(CompositeTargetInfo* target_info) {
  int ground_quad_size = ground_vertices_.size() / 4;
  if (!ground_quad_size)
    return;

  auto& shader = screen()->renderer()->GetPipelines()->tilemap2;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::Normal);
  screen()->renderer()->context()->SetPipelineState(pipeline->pso);

  shader.SetTexture(
      atlas_texture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  screen()->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Tilemap2::VSUniform>
        Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                  Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, target_info->viewport_size,
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->texSize = base::MakeInvert(base::Vec2i(
        atlas_texture_->GetDesc().Width, atlas_texture_->GetDesc().Height));
    Constants->transOffset = tilemap_offset_;
    Constants->tileSize = tile_size_;
    Constants->autotileAnimationOffset = animation_offset_;
  }

  tilemap_quads_->Draw(screen()->renderer()->context(), 0, ground_quad_size);
  DrawFlashLayerInternal(target_info);
}

void Tilemap2::DrawAboveLayerInternal(CompositeTargetInfo* target_info) {
  int ground_quad_size = ground_vertices_.size() / 4;
  int above_quad_size = above_vertices_.size() / 4;

  if (!above_quad_size)
    return;

  auto& shader = screen()->renderer()->GetPipelines()->base;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::Normal);
  screen()->renderer()->context()->SetPipelineState(pipeline->pso);

  shader.SetTexture(
      atlas_texture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  screen()->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Base::VSUniform> Constants(
        screen()->renderer()->context(), shader.GetVSUniform(),
        Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, target_info->viewport_size,
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->texSize = base::MakeInvert(base::Vec2i(
        atlas_texture_->GetDesc().Width, atlas_texture_->GetDesc().Height));
    Constants->transOffset = tilemap_offset_;
  }

  tilemap_quads_->Draw(screen()->renderer()->context(), ground_quad_size,
                       above_quad_size);
}

}  // namespace content