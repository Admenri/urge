// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TILEUTILS_H_
#define CONTENT_COMMON_TILEUTILS_H_

#include "content/public/drawable.h"
#include "content/public/graphics.h"
#include "content/public/table.h"
#include "renderer/drawable/quad_array.h"

namespace content {

template <typename T>
struct Sides {
  T left, right, top, bottom;
};

template <typename T>
struct Corners {
  T top_left, top_right, bottom_left, bottom_right;
};

inline int vwrap(int value, int range) {
  int res = value % range;
  return res < 0 ? res + range : res;
};

inline int16_t TableGetWrapped(scoped_refptr<content::Table> t,
                               int x,
                               int y,
                               int z = 0) {
  return t->Get(vwrap(x, t->GetXSize()), vwrap(y, t->GetYSize()), z);
}

inline int16_t TableGetFlag(scoped_refptr<content::Table> t, int x) {
  if (!t)
    return 0;

  if (x < 0 || x >= t->GetXSize())
    return 0;

  return t->At(x);
}

class TilemapFlashLayer {
 public:
  TilemapFlashLayer(int tile_size,
                    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
                    scoped_refptr<renderer::QuadArrayIndices> indices);
  ~TilemapFlashLayer() {}

  TilemapFlashLayer(const TilemapFlashLayer&) = delete;
  TilemapFlashLayer& operator=(const TilemapFlashLayer&) = delete;

  scoped_refptr<Table> GetFlashData() const { return flashdata_; }
  void SetFlashData(scoped_refptr<Table> data) {
    if (data == flashdata_)
      return;

    flashdata_ = data;
    buffer_need_update_ = true;

    observer_ = data->AddObserver(base::BindRepeating(
        &TilemapFlashLayer::SetUpdateBuffer, base::Unretained(this)));
  }

  void SetViewport(const base::Rect& viewport) {
    flash_viewport_ = viewport;
    buffer_need_update_ = true;
  }

  void BeforeComposite(
      Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context);
  void Composite(CompositeTargetInfo* target_info,
                 GraphicsHost* screen,
                 const base::Vec2i offset,
                 float alpha);

 private:
  void SetUpdateBuffer() { buffer_need_update_ = true; }
  bool SampleFlashColor(base::Vec4& out, int x, int y) const;
  void UpdateBuffer(Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context);

  scoped_refptr<Table> flashdata_;
  std::unique_ptr<renderer::QuadArray> quads_;
  base::Rect flash_viewport_;
  bool buffer_need_update_ = false;
  int tile_size_;

  base::CallbackListSubscription observer_;
};

}  // namespace content

#endif  // !CONTENT_COMMON_TILEUTILS_H_
