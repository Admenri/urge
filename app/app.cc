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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>

#define SDL_MAIN_USE_CALLBACKS
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_messagebox.h"

#include "app/platform/win32.h"

#include "core/config.h"
#include "core/filesystem.h"
#include "core/graphics.h"
#include "core/logger.h"
#include "core/sprite.h"
#include "core/uniform.h"

namespace {

using urge::Bitmap;
using urge::Color;
using urge::Graphics;
using urge::MakeRefCounted;
using urge::RefPtr;
using urge::Sprite;
using urge::UniformBlockPool;
using urge::UniformManager;
using urge::Vec4;

/* --------------------------------------------------------------------------
   Test scene

   The scene below is a fixed layout of sprites which stays in the application
   to make performance debugging possible: it covers every shape the sprite
   pipeline can draw -- a plain quad, a waved one, a mirrored one, a rotated and
   scaled one -- and then a grid of kGridSprites small sprites, which is what
   makes one frame need more slots than a uniform pool holds in its first chunk.

   The layout keeps the shapes apart, so the position of a pixel tells which
   sprite covers it, see kProbePoints below. Nothing of this belongs to the
   engine: the objects are released in SDL_AppQuit() and the pools are reported
   before that.
   -------------------------------------------------------------------------- */

//! The grid of small sprites: the sprites of a row are laid out next to each
//! other, a step of kGridStep pixels is the width of one of them.
constexpr int32_t kGridColumns = 30;
constexpr int32_t kGridRows = 10;
constexpr int32_t kGridSprites = kGridColumns * kGridRows;
constexpr int32_t kGridOriginX = 320;
constexpr int32_t kGridOriginY = 400;
constexpr int32_t kGridStep = 6;
constexpr float kGridZoom = 0.02f;

//! The frame the numeric probes run at, which the wave has moved by then.
constexpr int32_t kProbeFrame = 12;

/*! The source of every sprite: "test.png", next to the executable. The mirror
    probe compares a pixel of a snapshot against the texels of this bitmap, so
    it is kept beside the sprites instead of only inside them. */
RefPtr<Bitmap> test_bitmap;
//! A plain quad at (10, 10): the unmodified sprite, the reference of the rest.
RefPtr<Sprite> test_plain;
//! A waved sprite at (330, 10): it is bent in strips of 8 pixels sideways.
RefPtr<Sprite> test_wave;
//! A mirrored sprite at (10, 250), which reverses its texture coordinates.
RefPtr<Sprite> test_mirror;
/*! A sprite at (400, 300) which is rotated by 30 degrees around its origin and
    halved, so its covered area is not a rectangle anymore. */
RefPtr<Sprite> test_rotated;
//! The grid of small sprites, at (kGridOriginX, kGridOriginY) and beyond.
RefPtr<Sprite> test_grid[kGridSprites];

//! The frames the application ran, the probes wait for kProbeFrame of them.
int32_t frame_counter = 0;
//! True once the probes ran, they are only interested in one frame.
bool probe_done = false;

//! One pixel of the scene, and whether a sprite has to cover it.
struct ProbePoint {
  //! A description of what the point is expected to show.
  const char* label;
  int32_t x;
  int32_t y;
  //! True when a sprite has to reach the point, false when it has to stay black.
  bool covered;
};

/*! Points which lie inside exactly one shape of the scene or inside none of
    them, so the color of a snapshot answers whether the shape was rasterized
    where it belongs, see IsCovered().

    The layout of the scene makes every point unambiguous: the plain quad covers
    [10, 311) x [10, 244), the wave one [322, 640) x [10, 244), the mirrored one
    [10, 311) x [250, 480), the rotated one a diamond of 189 x 177 pixels around
    (400, 300) and the grid [320, 500) x [400, 460). */
const ProbePoint kProbePoints[] = {
    {"plain, first pixel", 10, 10, true},
    {"plain, above and left of it", 9, 9, false},
    {"plain, right of its last column", 311, 244, false},
    {"wave, inside a strip", 600, 20, true},
    {"rotated, around its origin", 400, 300, true},
    {"rotated, right of its widest row", 510, 300, false},
    {"rotated, upper right corner", 430, 230, true},
    {"mirror, inside", 15, 255, true},
    {"grid, first sprite", 323, 402, true},
    {"grid, last sprite", 496, 456, true},
    {"empty, lower right corner", 620, 470, false},
};

//! Reports a color the way a probe reads it, in the 0 to 255 scale of the
//! Color class rather than in the 0 to 1 scale of a shader.
std::string Describe(const Vec4& color) {
  return std::format("rgba({:.0f}, {:.0f}, {:.0f}, {:.0f})", color.r, color.g,
                     color.b, color.a);
}

//! The largest difference of two colors over their four channels, which is zero
//! for two colors of the same pixel.
float ColorDistance(const Vec4& lhs, const Vec4& rhs) {
  return std::max({std::abs(lhs.r - rhs.r), std::abs(lhs.g - rhs.g),
                   std::abs(lhs.b - rhs.b), std::abs(lhs.a - rhs.a)});
}

/*! Whether a color of a snapshot is the one a sprite drew.

    The target of a snapshot is cleared with an opaque black, so the alpha
    channel is opaque on a covered pixel as well and cannot tell the two apart:
    a pixel is covered when it is not black. A sprite of the scene which is
    covered by a transparent texel of the bitmap leaves black behind as well,
    which is what the layout of the scene avoids, see kProbePoints. */
bool IsCovered(const Vec4& color) {
  return color.r > 0.0f || color.g > 0.0f || color.b > 0.0f;
}

//! Creates the sprites of the test scene on top of the screen root. A sprite
//! whose parent is not set draws into the root, see Node::Attr_Parent.
void CreateTestScene() {
  test_bitmap = MakeRefCounted<Bitmap>("test.png");

  test_plain = MakeRefCounted<Sprite>();
  test_plain->Attr_Bitmap(test_bitmap);
  test_plain->Attr_X(10);
  test_plain->Attr_Y(10);

  /* A wave keeps its amplitude in pixels and its length in the pixels of a
     whole wave: the sprite is drawn as strips of 8 pixels which are moved
     sideways by a sine, so the layout of the geometry changes every frame as
     Update() advances the phase. */
  test_wave = MakeRefCounted<Sprite>();
  test_wave->Attr_Bitmap(test_bitmap);
  test_wave->Attr_X(330);
  test_wave->Attr_Y(10);
  test_wave->Attr_WaveAmp(8);
  test_wave->Attr_WaveLength(30);
  test_wave->Attr_Mirror(true);
  test_wave->Attr_Opacity(180);

  test_mirror = MakeRefCounted<Sprite>();
  test_mirror->Attr_Bitmap(test_bitmap);
  test_mirror->Attr_X(10);
  test_mirror->Attr_Y(250);
  test_mirror->Attr_Mirror(true);

  /* The origin of a sprite is the point it is scaled and rotated around, so the
     one here is the center of the source bitmap, which puts the rotated shape
     around (400, 300) instead of around its upper left corner. */
  test_rotated = MakeRefCounted<Sprite>();
  test_rotated->Attr_Bitmap(test_bitmap);
  test_rotated->Attr_X(400);
  test_rotated->Attr_Y(300);
  test_rotated->Attr_OX(150);
  test_rotated->Attr_OY(117);
  test_rotated->Attr_Angle(30.0f);
  test_rotated->Attr_ZoomX(0.5f);
  test_rotated->Attr_ZoomY(0.5f);

  for (int32_t index = 0; index < kGridSprites; ++index) {
    const int32_t column = index % kGridColumns;
    const int32_t row = index / kGridColumns;

    RefPtr<Sprite>& sprite = test_grid[index];
    sprite = MakeRefCounted<Sprite>();
    sprite->Attr_Bitmap(test_bitmap);
    sprite->Attr_X(kGridOriginX + column * kGridStep);
    sprite->Attr_Y(kGridOriginY + row * kGridStep);
    sprite->Attr_ZoomX(kGridZoom);
    sprite->Attr_ZoomY(kGridZoom);
    sprite->Attr_Opacity(96);
  }
}

//! Releases the test scene. It runs while the device is alive, so the buffers
//! of the sprites are destroyed before the pools and the device are.
void DestroyTestScene() {
  for (int32_t index = 0; index < kGridSprites; ++index)
    test_grid[index].reset();

  test_rotated.reset();
  test_mirror.reset();
  test_wave.reset();
  test_plain.reset();
  test_bitmap.reset();

  frame_counter = 0;
  probe_done = false;
}

//! Reports the geometry and the occupancy of one pool of the uniform manager.
void ReportPool(const UniformBlockPool& pool) {
  std::string occupancy;
  for (std::size_t index = 0; index < pool.chunk_count(); ++index) {
    if (index > 0)
      occupancy += ", ";
    occupancy += std::to_string(pool.chunk(index).used);
  }

  LOGGER_INFO(
      "uniform pool '{}': {} bytes per element, {} bytes per slot, {} slots per "
      "chunk of {} bytes, {} chunks, slots used [{}]",
      pool.name(), pool.element_size(), pool.slot_stride(),
      pool.slots_per_chunk(), pool.chunk_size(), pool.chunk_count(), occupancy);
}

//! Reports both pools of the engine, see ReportPool().
void ReportPools() {
  UniformManager& uniforms = UniformManager::Get();
  ReportPool(uniforms.object_uniforms());
  ReportPool(uniforms.sprite_uniforms());
}

/*! Compares the pixel the mirrored sprite draws at (15, 255) against the source
    texels it could have taken it from.

    The sprite is at (10, 250) and draws its source rectangle backwards, so the
    pixel at screen (15, 255) reads the texel at (295, 5) of the bitmap and the
    distance to it has to be zero, while the texel at (5, 5) would be the one a
    sprite which is not mirrored would read -- the distance to it is the error
    the probe detects. Both colors are reported, so a source texel which happens
    to be black cannot make the comparison hold for any geometry. */
void ProbeMirror(RefPtr<Bitmap> snap) {
  const Vec4 drawn = snap->GetPixel(15, 255)->data;
  const Vec4 mirrored = test_bitmap->GetPixel(295, 5)->data;
  const Vec4 direct = test_bitmap->GetPixel(5, 5)->data;

  LOGGER_INFO(
      "probe mirror: drawn {} against mirrored {} (distance {:.1f}) and "
      "against direct {} (distance {:.1f})",
      Describe(drawn), Describe(mirrored), ColorDistance(drawn, mirrored),
      Describe(direct), ColorDistance(drawn, direct));
}

/*! Runs the probes of a frame: the pixels of the scene and the occupancy of the
    uniform pools.

    The snapshot renders the tree off screen once more, so the pixels can be
    read back from the host and compared with what the layout of the scene says
    they have to show, and the pools are reported afterwards because that render
    is what filled them for the frame being inspected. */
void ProbeFrame() {
  RefPtr<Bitmap> snap = Graphics::Get().SnapToBitmap();

  for (const ProbePoint& point : kProbePoints) {
    const RefPtr<Color> color = snap->GetPixel(point.x, point.y);
    const bool covered = IsCovered(color->data);

    LOGGER_INFO("probe {}: ({}, {}) is {}, expected {}, {}", point.label,
                point.x, point.y, Describe(color->data),
                point.covered ? "covered" : "empty",
                covered == point.covered ? "ok" : "MISMATCH");
  }

  ProbeMirror(snap);
  ReportPools();
}

}  // namespace

SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char* argv[]) {
  // App name
  std::string app(argv[0]);
  for (size_t i = 0; i < app.size(); ++i)
    if (app[i] == '\\')
      app[i] = '/';

  // Game directory (where the executable lives)
  std::string base_dir = ".";
  auto last_sep = app.find_last_of('/');
  if (last_sep != std::string::npos) {
    base_dir = app.substr(0, last_sep);
    app = app.substr(last_sep + 1);
  }

  last_sep = app.find_last_of('.');
  if (last_sep != std::string::npos)
    app = app.substr(0, last_sep);
  std::string ini = app + ".ini";

  // Components initialize
  try {
    auto io = new urge::IOService(argv[0]);
    urge::IOService::Reset(io);
    io->SetWritePath(base_dir);
    io->AddLoadPath(".", "/");

    auto config = new urge::Config(ini);
    urge::Config::Reset(config);

    auto graphics = new urge::Graphics();
    urge::Graphics::Reset(graphics);

    // The test scene draws into the root of the screen, so the graphics above
    // have to exist before it is created
    CreateTestScene();
  } catch (urge::Exception exc) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "URGE Core",
                             exc.message().c_str(), nullptr);
    return SDL_APP_FAILURE;
  }

  // SDL only enters the main loop when SDL_AppInit returns SDL_APP_CONTINUE
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate) {
  ++frame_counter;

  // The wave of a sprite is what advances with the frames it is updated on
  test_wave->Update();

  urge::Graphics::Get().Update();

  if (frame_counter == kProbeFrame && !probe_done) {
    probe_done = true;
    ProbeFrame();
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event* event) {
  if (event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;
  return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result) {
  DestroyTestScene();
  urge::Graphics::Reset(nullptr);
  urge::Config::Reset(nullptr);
  urge::IOService::Reset(nullptr);
}
