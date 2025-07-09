// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_TILEMAPBITMAP_H_
#define CONTENT_PUBLIC_ENGINE_TILEMAPBITMAP_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_bitmap.h"
#include "content/public/engine_table.h"
#include "content/public/engine_viewport.h"

namespace content {

/*--urge(name:TilemapBitmap)--*/
class URGE_OBJECT(TilemapBitmap) {
 public:
  virtual ~TilemapBitmap() = default;

  /*--urge(name:[])--*/
  virtual scoped_refptr<Bitmap> Get(int32_t index,
                                    ExceptionState& exception_state) = 0;

  /*--urge(name:[]=)--*/
  virtual void Put(int32_t index,
                   scoped_refptr<Bitmap> texture,
                   ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_TILEMAPBITMAP_H_
