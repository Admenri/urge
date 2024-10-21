// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/tilemap.h"

#include "content/common/tileutils.h"

namespace content {

namespace {

using AutotileSubPos = enum {
  TopLeft = 0,
  TopRight,
  BottomLeft,
  BottomRight,
};

const base::RectF kAutotileSrcRects[] = {
    {1, 2, 0.5, 0.5},     {1.5, 2, 0.5, 0.5},   {1, 2.5, 0.5, 0.5},
    {1.5, 2.5, 0.5, 0.5}, {2, 0, 0.5, 0.5},     {1.5, 2, 0.5, 0.5},
    {1, 2.5, 0.5, 0.5},   {1.5, 2.5, 0.5, 0.5}, {1, 2, 0.5, 0.5},
    {2.5, 0, 0.5, 0.5},   {1, 2.5, 0.5, 0.5},   {1.5, 2.5, 0.5, 0.5},
    {2, 0, 0.5, 0.5},     {2.5, 0, 0.5, 0.5},   {1, 2.5, 0.5, 0.5},
    {1.5, 2.5, 0.5, 0.5}, {1, 2, 0.5, 0.5},     {1.5, 2, 0.5, 0.5},
    {1, 2.5, 0.5, 0.5},   {2.5, 0.5, 0.5, 0.5}, {2, 0, 0.5, 0.5},
    {1.5, 2, 0.5, 0.5},   {1, 2.5, 0.5, 0.5},   {2.5, 0.5, 0.5, 0.5},
    {1, 2, 0.5, 0.5},     {2.5, 0, 0.5, 0.5},   {1, 2.5, 0.5, 0.5},
    {2.5, 0.5, 0.5, 0.5}, {2, 0, 0.5, 0.5},     {2.5, 0, 0.5, 0.5},
    {1, 2.5, 0.5, 0.5},   {2.5, 0.5, 0.5, 0.5}, {1, 2, 0.5, 0.5},
    {1.5, 2, 0.5, 0.5},   {2, 0.5, 0.5, 0.5},   {1.5, 2.5, 0.5, 0.5},
    {2, 0, 0.5, 0.5},     {1.5, 2, 0.5, 0.5},   {2, 0.5, 0.5, 0.5},
    {1.5, 2.5, 0.5, 0.5}, {1, 2, 0.5, 0.5},     {2.5, 0, 0.5, 0.5},
    {2, 0.5, 0.5, 0.5},   {1.5, 2.5, 0.5, 0.5}, {2, 0, 0.5, 0.5},
    {2.5, 0, 0.5, 0.5},   {2, 0.5, 0.5, 0.5},   {1.5, 2.5, 0.5, 0.5},
    {1, 2, 0.5, 0.5},     {1.5, 2, 0.5, 0.5},   {2, 0.5, 0.5, 0.5},
    {2.5, 0.5, 0.5, 0.5}, {2, 0, 0.5, 0.5},     {1.5, 2, 0.5, 0.5},
    {2, 0.5, 0.5, 0.5},   {2.5, 0.5, 0.5, 0.5}, {1, 2, 0.5, 0.5},
    {2.5, 0, 0.5, 0.5},   {2, 0.5, 0.5, 0.5},   {2.5, 0.5, 0.5, 0.5},
    {2, 0, 0.5, 0.5},     {2.5, 0, 0.5, 0.5},   {2, 0.5, 0.5, 0.5},
    {2.5, 0.5, 0.5, 0.5}, {0, 2, 0.5, 0.5},     {0.5, 2, 0.5, 0.5},
    {0, 2.5, 0.5, 0.5},   {0.5, 2.5, 0.5, 0.5}, {0, 2, 0.5, 0.5},
    {2.5, 0, 0.5, 0.5},   {0, 2.5, 0.5, 0.5},   {0.5, 2.5, 0.5, 0.5},
    {0, 2, 0.5, 0.5},     {0.5, 2, 0.5, 0.5},   {0, 2.5, 0.5, 0.5},
    {2.5, 0.5, 0.5, 0.5}, {0, 2, 0.5, 0.5},     {2.5, 0, 0.5, 0.5},
    {0, 2.5, 0.5, 0.5},   {2.5, 0.5, 0.5, 0.5}, {1, 1, 0.5, 0.5},
    {1.5, 1, 0.5, 0.5},   {1, 1.5, 0.5, 0.5},   {1.5, 1.5, 0.5, 0.5},
    {1, 1, 0.5, 0.5},     {1.5, 1, 0.5, 0.5},   {1, 1.5, 0.5, 0.5},
    {2.5, 0.5, 0.5, 0.5}, {1, 1, 0.5, 0.5},     {1.5, 1, 0.5, 0.5},
    {2, 0.5, 0.5, 0.5},   {1.5, 1.5, 0.5, 0.5}, {1, 1, 0.5, 0.5},
    {1.5, 1, 0.5, 0.5},   {2, 0.5, 0.5, 0.5},   {2.5, 0.5, 0.5, 0.5},
    {2, 2, 0.5, 0.5},     {2.5, 2, 0.5, 0.5},   {2, 2.5, 0.5, 0.5},
    {2.5, 2.5, 0.5, 0.5}, {2, 2, 0.5, 0.5},     {2.5, 2, 0.5, 0.5},
    {2, 0.5, 0.5, 0.5},   {2.5, 2.5, 0.5, 0.5}, {2, 0, 0.5, 0.5},
    {2.5, 2, 0.5, 0.5},   {2, 2.5, 0.5, 0.5},   {2.5, 2.5, 0.5, 0.5},
    {2, 0, 0.5, 0.5},     {2.5, 2, 0.5, 0.5},   {2, 0.5, 0.5, 0.5},
    {2.5, 2.5, 0.5, 0.5}, {1, 3, 0.5, 0.5},     {1.5, 3, 0.5, 0.5},
    {1, 3.5, 0.5, 0.5},   {1.5, 3.5, 0.5, 0.5}, {2, 0, 0.5, 0.5},
    {1.5, 3, 0.5, 0.5},   {1, 3.5, 0.5, 0.5},   {1.5, 3.5, 0.5, 0.5},
    {1, 3, 0.5, 0.5},     {2.5, 0, 0.5, 0.5},   {1, 3.5, 0.5, 0.5},
    {1.5, 3.5, 0.5, 0.5}, {2, 0, 0.5, 0.5},     {2.5, 0, 0.5, 0.5},
    {1, 3.5, 0.5, 0.5},   {1.5, 3.5, 0.5, 0.5}, {0, 2, 0.5, 0.5},
    {2.5, 2, 0.5, 0.5},   {0, 2.5, 0.5, 0.5},   {2.5, 2.5, 0.5, 0.5},
    {1, 1, 0.5, 0.5},     {1.5, 1, 0.5, 0.5},   {1, 3.5, 0.5, 0.5},
    {1.5, 3.5, 0.5, 0.5}, {0, 1, 0.5, 0.5},     {0.5, 1, 0.5, 0.5},
    {0, 1.5, 0.5, 0.5},   {0.5, 1.5, 0.5, 0.5}, {0, 1, 0.5, 0.5},
    {0.5, 1, 0.5, 0.5},   {0, 1.5, 0.5, 0.5},   {2.5, 0.5, 0.5, 0.5},
    {2, 1, 0.5, 0.5},     {2.5, 1, 0.5, 0.5},   {2, 1.5, 0.5, 0.5},
    {2.5, 1.5, 0.5, 0.5}, {2, 1, 0.5, 0.5},     {2.5, 1, 0.5, 0.5},
    {2, 0.5, 0.5, 0.5},   {2.5, 1.5, 0.5, 0.5}, {2, 3, 0.5, 0.5},
    {2.5, 3, 0.5, 0.5},   {2, 3.5, 0.5, 0.5},   {2.5, 3.5, 0.5, 0.5},
    {2, 0, 0.5, 0.5},     {2.5, 3, 0.5, 0.5},   {2, 3.5, 0.5, 0.5},
    {2.5, 3.5, 0.5, 0.5}, {0, 3, 0.5, 0.5},     {0.5, 3, 0.5, 0.5},
    {0, 3.5, 0.5, 0.5},   {0.5, 3.5, 0.5, 0.5}, {0, 3, 0.5, 0.5},
    {2.5, 0, 0.5, 0.5},   {0, 3.5, 0.5, 0.5},   {0.5, 3.5, 0.5, 0.5},
    {0, 1, 0.5, 0.5},     {2.5, 1, 0.5, 0.5},   {0, 1.5, 0.5, 0.5},
    {2.5, 1.5, 0.5, 0.5}, {0, 1, 0.5, 0.5},     {0.5, 1, 0.5, 0.5},
    {0, 3.5, 0.5, 0.5},   {0.5, 3.5, 0.5, 0.5}, {0, 3, 0.5, 0.5},
    {2.5, 3, 0.5, 0.5},   {0, 3.5, 0.5, 0.5},   {2.5, 3.5, 0.5, 0.5},
    {2, 1, 0.5, 0.5},     {2.5, 1, 0.5, 0.5},   {2, 3.5, 0.5, 0.5},
    {2.5, 3.5, 0.5, 0.5}, {0, 1, 0.5, 0.5},     {2.5, 1, 0.5, 0.5},
    {0, 3.5, 0.5, 0.5},   {2.5, 3.5, 0.5, 0.5}, {0, 0, 0.5, 0.5},
    {0.5, 0, 0.5, 0.5},   {0, 0.5, 0.5, 0.5},   {0.5, 0.5, 0.5, 0.5},
};

const uint8_t kAutotileAnimation[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

const int kAutotileAnimationSize =
    sizeof(kAutotileAnimation) / sizeof(kAutotileAnimation[0]);

const uint8_t kFlashAlpha[] = {
    /* Fade in */
    0x3C, 0x3C, 0x3C, 0x3C, 0x4B, 0x4B, 0x4B, 0x4B, 0x5A, 0x5A, 0x5A, 0x5A,
    0x69, 0x69, 0x69, 0x69,
    /* Fade out */
    0x78, 0x78, 0x78, 0x78, 0x69, 0x69, 0x69, 0x69, 0x5A, 0x5A, 0x5A, 0x5A,
    0x4B, 0x4B, 0x4B, 0x4B};

const int kFlashAlphaSize = sizeof(kFlashAlpha) / sizeof(kFlashAlpha[0]);

const int kGroundLayerDefaultZ = 0;
const int kZLayerDefaultZ = 1;

}  // namespace

class TilemapGroundLayer : public ViewportChild {
 public:
  TilemapGroundLayer(scoped_refptr<Graphics> screen, Tilemap* tilemap)
      : ViewportChild(screen, tilemap->viewport_, kGroundLayerDefaultZ),
        tilemap_(tilemap) {}

  TilemapGroundLayer(const TilemapGroundLayer&) = delete;
  TilemapGroundLayer& operator=(const TilemapGroundLayer&) = delete;

  void PrepareDraw() override { tilemap_->BeforeTilemapComposite(); }
  void OnDraw(CompositeTargetInfo* target_info) override {
    tilemap_->DrawGroundLayerInternal(target_info);
  }

  void CheckObjectDisposed() const override { tilemap_->CheckIsDisposed(); }
  void OnParentViewportRectChanged(
      const DrawableParent::ViewportInfo& rect) override {
    tilemap_->map_buffer_need_update_ = true;
  }

 private:
  Tilemap* tilemap_;
};

class TilemapZLayer : public ViewportChild {
 public:
  TilemapZLayer(scoped_refptr<Graphics> screen, Tilemap* tilemap)
      : ViewportChild(screen, tilemap->viewport_, kZLayerDefaultZ),
        tilemap_(tilemap) {}

  TilemapZLayer(const TilemapZLayer&) = delete;
  TilemapZLayer& operator=(const TilemapZLayer&) = delete;

  void CheckObjectDisposed() const override { tilemap_->CheckIsDisposed(); }
  void SetIndex(int index) { index_ = index; }
  void OnDraw(CompositeTargetInfo* target_info) override {
    tilemap_->DrawAboveLayerInternal(index_, target_info);
  }

 private:
  Tilemap* tilemap_;
  int index_ = 0;
};

Tilemap::Tilemap(scoped_refptr<Graphics> screen,
                 scoped_refptr<Viewport> viewport,
                 int tilesize)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      viewport_(viewport),
      tile_size_(tilesize) {
  InitTilemapData();
}

Tilemap::~Tilemap() {
  Dispose();
}

void Tilemap::Update() {
  CheckIsDisposed();

  frame_index_ = kAutotileAnimation[animation_index_];
  if (++animation_index_ >= kAutotileAnimationSize)
    animation_index_ = 0;

  if (++flash_alpha_index_ >= kFlashAlphaSize)
    flash_alpha_index_ = 0;
}

scoped_refptr<Bitmap> Tilemap::GetTileset() const {
  CheckIsDisposed();
  return tileset_;
}

void Tilemap::SetTileset(scoped_refptr<Bitmap> bitmap) {
  CheckIsDisposed();

  if (tileset_ == bitmap)
    return;

  tileset_ = bitmap;
  atlas_need_update_ = true;
  if (IsObjectValid(tileset_.get()))
    tileset_->AddBitmapObserver(base::BindRepeating(
        &Tilemap::RaiseUpdateAtlasInternal, base::Unretained(this)));
}

scoped_refptr<Bitmap> Tilemap::GetAutotiles(int index) const {
  CheckIsDisposed();
  return autotiles_[index].bitmap;
}

void Tilemap::SetAutotiles(int index, scoped_refptr<Bitmap> bitmap) {
  CheckIsDisposed();

  if (autotiles_[index].bitmap == bitmap)
    return;

  autotiles_[index].bitmap = bitmap;
  atlas_need_update_ = true;
  if (IsObjectValid(autotiles_[index].bitmap.get()))
    autotiles_[index].bitmap->AddBitmapObserver(base::BindRepeating(
        &Tilemap::RaiseUpdateAtlasInternal, base::Unretained(this)));
}

scoped_refptr<Table> Tilemap::GetMapData() const {
  CheckIsDisposed();
  return map_data_;
}

void Tilemap::SetMapData(scoped_refptr<Table> map_data) {
  CheckIsDisposed();

  if (map_data_ == map_data)
    return;

  map_data_ = map_data;
  map_buffer_need_update_ = true;
  if (map_data_)
    map_data_->AddObserver(base::BindRepeating(
        &Tilemap::RaiseUpdateBufferInternal, base::Unretained(this)));
}

scoped_refptr<Table> Tilemap::GetFlashData() const {
  CheckIsDisposed();
  return flash_data_;
}

void Tilemap::SetFlashData(scoped_refptr<Table> flash_data) {
  CheckIsDisposed();

  if (flash_data_ == flash_data)
    return;

  flash_data_ = flash_data;
  flash_layer_->SetFlashData(flash_data_);
}

scoped_refptr<Table> Tilemap::GetPriorities() const {
  CheckIsDisposed();
  return priorities_;
}

void Tilemap::SetPriorities(scoped_refptr<Table> flags) {
  CheckIsDisposed();

  if (priorities_ == flags)
    return;

  priorities_ = flags;
  map_buffer_need_update_ = true;
  if (priorities_)
    priorities_->AddObserver(base::BindRepeating(
        &Tilemap::RaiseUpdateBufferInternal, base::Unretained(this)));
}

scoped_refptr<Viewport> Tilemap::GetViewport() const {
  CheckIsDisposed();
  return viewport_;
}

bool Tilemap::GetVisible() const {
  CheckIsDisposed();
  return visible_;
}

void Tilemap::SetVisible(bool visible) {
  CheckIsDisposed();

  if (visible_ == visible)
    return;
  visible_ = visible;

  ground_layer_->SetVisible(visible_);
  for (auto& it : above_layers_)
    it->SetVisible(visible_);
}

int Tilemap::GetOX() const {
  CheckIsDisposed();
  return origin_.x;
}

void Tilemap::SetOX(int ox) {
  CheckIsDisposed();

  if (origin_.x == ox)
    return;
  origin_.x = ox;

  map_buffer_need_update_ = true;
}

int Tilemap::GetOY() const {
  CheckIsDisposed();
  return origin_.y;
}

void Tilemap::SetOY(int oy) {
  CheckIsDisposed();

  if (origin_.y == oy)
    return;
  origin_.y = oy;

  map_buffer_need_update_ = true;
}

void Tilemap::OnObjectDisposed() {
  ground_layer_.reset();
  above_layers_.clear();

  tilemap_quads_.reset();
  flash_layer_.reset();
  atlas_texture_.Release();
}

void Tilemap::InitTilemapData() {
  flash_layer_.reset(
      new TilemapFlashLayer(tile_size_, screen()->renderer()->device(),
                            screen()->renderer()->quad_index_buffer()));
  tilemap_quads_.reset(
      new renderer::QuadArray(screen()->renderer()->device(),
                              screen()->renderer()->quad_index_buffer()));

  ground_layer_.reset(
      new TilemapGroundLayer(static_cast<Graphics*>(screen()), this));
  SetupDrawLayersInternal();
}

void Tilemap::BeforeTilemapComposite() {
  flash_layer_->BeforeComposite(screen()->renderer()->context());

  UpdateViewportInternal(ground_layer_->parent_rect());
  ResetZLayerInternal();

  if (atlas_need_update_) {
    atlas_need_update_ = false;
    MakeAtlasInternal();
  }

  if (map_buffer_need_update_) {
    map_buffer_need_update_ = false;
    UpdateMapBufferInternal();
  }
}

void Tilemap::MakeAtlasInternal() {
  int atlas_height = 28 * tile_size_;
  if (IsObjectValid(tileset_.get()))
    atlas_height = std::max(atlas_height, tileset_->GetSize().y);

  {
    Diligent::TextureDesc TexDesc;
    TexDesc.Name = "TilemapXP atlas texture";
    TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    TexDesc.Format = screen()->tex_format();
    TexDesc.Usage = Diligent::USAGE_DEFAULT;
    TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    TexDesc.BindFlags =
        Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
    TexDesc.Width = tile_size_ * 20;
    TexDesc.Height = atlas_height;

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

  // Autotile part
  int offset = 0;
  for (auto& it : autotiles_) {
    if (!IsObjectValid(it.bitmap.get())) {
      ++offset;
      continue;
    }

    auto autotile_size = it.bitmap->GetSize();

    base::Rect dst_pos(autotile_size);
    dst_pos.x = 0;
    dst_pos.y = offset * tile_size_ * 4;

    if (autotile_size.x > 3 * tile_size_ && autotile_size.y > tile_size_) {
      // Animated autotile
      it.type = AutotileType::Animated;
    } else if (autotile_size.x <= 3 * tile_size_ &&
               autotile_size.y > tile_size_) {
      // Static autotile
      dst_pos.x += tile_size_ * 3;
      it.type = AutotileType::Static;
    } else if (autotile_size.x <= 4 * tile_size_ &&
               autotile_size.y <= tile_size_) {
      // Single animated tile
      it.type = AutotileType::SingleAnimated;

      for (int i = 0; i < 4; ++i) {
        Diligent::Box SrcBox(tile_size_ * i, tile_size_ * i + tile_size_, 0,
                             tile_size_);
        renderer::ClampBox(&SrcBox, it.bitmap->GetSize());
        Diligent::CopyTextureAttribs CopyTexAttr(
            it.bitmap->GetHandle(),
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, atlas_texture_,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        CopyTexAttr.pSrcBox = &SrcBox;
        CopyTexAttr.DstX = tile_size_ * i * 3;
        CopyTexAttr.DstY = 0;
        screen()->renderer()->context()->CopyTexture(CopyTexAttr);
      }

      ++offset;
      continue;
    } else {
      NOTREACHED();
    }

    Diligent::Box SrcBox(0, autotile_size.x, 0, autotile_size.y);
    renderer::ClampBox(&SrcBox, it.bitmap->GetSize());
    Diligent::CopyTextureAttribs CopyTexAttr(
        it.bitmap->GetHandle(),
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, atlas_texture_,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    CopyTexAttr.pSrcBox = &SrcBox;
    CopyTexAttr.DstX = dst_pos.x;
    CopyTexAttr.DstY = dst_pos.y;
    screen()->renderer()->context()->CopyTexture(CopyTexAttr);

    ++offset;
  }

  // Tileset part
  if (!IsObjectValid(tileset_.get()))
    return;

  auto tileset_size = tileset_->GetSize();
  base::Rect dst_rect = tileset_size;
  dst_rect.x = 12 * tile_size_;
  dst_rect.y = 0;

  Diligent::Box SrcBox(0, tileset_size.x, 0, tileset_size.y);
  renderer::ClampBox(&SrcBox, tileset_->GetSize());
  Diligent::CopyTextureAttribs CopyTexAttr(
      tileset_->GetHandle(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, atlas_texture_,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  CopyTexAttr.pSrcBox = &SrcBox;
  CopyTexAttr.DstX = dst_rect.x;
  CopyTexAttr.DstY = dst_rect.y;
  screen()->renderer()->context()->CopyTexture(CopyTexAttr);
}

void Tilemap::UpdateViewportInternal(
    const DrawableParent::ViewportInfo& viewport_rect) {
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
    map_buffer_need_update_ = true;

    flash_layer_->SetViewport(tilemap_viewport_);
  }

  const base::Vec2i display_offset =
      tilemap_origin % base::Vec2i(tile_size_, tile_size_);
  tilemap_offset_ = viewport_rect.rect.Position() - display_offset -
                    base::Vec2i(0, tile_size_);
}

void Tilemap::UpdateMapBufferInternal() {
  ground_vertices_.clear();
  above_vertices_.clear();

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
      default:
        break;
    }
  };

  auto get_priority = [&](int tileID) -> int {
    if (!priorities_ || tileID >= priorities_->GetXSize())
      return 0;

    int value = priorities_->At(tileID);
    if (value > 5)
      return -1;
    return value;
  };

  auto process_autotile =
      [&](int x, int y, int tileID,
          std::vector<renderer::GeometryVertexLayout::Data>* target) {
        // autotile index 0-7
        int autotileID = tileID / 48 - 1;
        // pattern id (0-47)
        int patternID = tileID % 48;

        AutotileInfo& info = autotiles_[autotileID];
        if (!IsObjectValid(info.bitmap.get()))
          return;

        switch (info.type) {
          case AutotileType::Animated:
          case AutotileType::Static: {
            const base::RectF* autotile_src = &kAutotileSrcRects[patternID * 4];
            for (int i = 0; i < 4; ++i) {
              base::RectF tex_src = autotile_src[i];
              tex_src.x = tex_src.x * tile_size_ + 0.5f;
              tex_src.y = tex_src.y * tile_size_ + 0.5f;
              tex_src.width = tex_src.width * tile_size_ - 1.0f;
              tex_src.height = tex_src.height * tile_size_ - 1.0f;

              if (info.type == AutotileType::Static)
                tex_src.x += 3 * tile_size_;
              tex_src.y += autotileID * 4 * tile_size_;

              base::RectF chunk_pos(x * tile_size_, y * tile_size_,
                                    tile_size_ / 2.0f, tile_size_ / 2.0f);
              autotile_subpos(chunk_pos, i);

              renderer::GeometryVertexLayout::Data verts[4];
              renderer::GeometryVertexLayout::SetTexPos(verts, tex_src,
                                                        chunk_pos);
              for (int j = 0; j < 4; ++j)
                target->push_back(verts[j]);
            }
          } break;
          case AutotileType::SingleAnimated: {
            renderer::GeometryVertexLayout::Data verts[4];
            const base::RectF single_tex(0.5f,
                                         0.5f + autotileID * tile_size_ * 4.0f,
                                         tile_size_ - 1.0f, tile_size_ - 1.0f);
            const base::RectF single_pos(x * tile_size_, y * tile_size_,
                                         tile_size_, tile_size_);
            renderer::GeometryVertexLayout::SetTexPos(verts, single_tex,
                                                      single_pos);
            for (int i = 0; i < 4; ++i)
              target->push_back(verts[i]);
          } break;
          default:
            NOTREACHED();
            break;
        }
      };

  auto process_tile = [&](int x, int y, int z) {
    int tileID = TableGetWrapped(map_data_, x + tilemap_viewport_.x,
                                 y + tilemap_viewport_.y, z);

    if (tileID < 48)
      return;

    int priority = get_priority(tileID);
    if (priority == -1)
      return;

    std::vector<renderer::GeometryVertexLayout::Data>* target;
    if (!priority) {
      // Ground layer
      target = &ground_vertices_;
    } else {
      // Above multi layers
      target = &above_vertices_[y + priority];
    }

    if (tileID < 48 * 8)
      return process_autotile(x, y, tileID, target);

    int tilesetID = tileID - 48 * 8;
    int tileX = tilesetID % 8;
    int tileY = tilesetID / 8;

    base::Vec2i atlas_offset(12 + tileX, tileY);
    base::RectF tex((float)atlas_offset.x * tile_size_ + 0.5f,
                    (float)atlas_offset.y * tile_size_ + 0.5f,
                    (float)tile_size_ - 1.0f, (float)tile_size_ - 1.0f);
    base::RectF pos(x * tile_size_, y * tile_size_, tile_size_, tile_size_);
    renderer::GeometryVertexLayout::Data verts[4];
    renderer::GeometryVertexLayout::SetTexPos(verts, tex, pos);

    for (int i = 0; i < 4; ++i)
      target->push_back(verts[i]);
  };

  auto process_buffer = [&]() {
    for (int x = 0; x < tilemap_viewport_.width; ++x)
      for (int y = 0; y < tilemap_viewport_.height; ++y)
        for (int z = 0; z < map_data_->GetZSize(); ++z)
          process_tile(x, y, z);
  };

  // Process map buffer
  int zlayer_num = tilemap_viewport_.height + 5;
  above_vertices_.resize(zlayer_num);

  // Begin to parse buffer data
  process_buffer();

  // Update quads
  int vertex_count = ground_vertices_.size();
  for (auto& it : above_vertices_)
    vertex_count += it.size();
  tilemap_quads_->Resize(screen()->renderer()->context(), vertex_count / 4);
  if (!vertex_count)
    return;

  above_offsets_.clear();
  int vert_add = 0;
  renderer::GeometryVertexLayout::Data* vert =
      tilemap_quads_->vertices().data();
  memcpy(
      vert, ground_vertices_.data(),
      ground_vertices_.size() * sizeof(renderer::GeometryVertexLayout::Data));
  vert += ground_vertices_.size();
  for (auto& it : above_vertices_) {
    memcpy(vert, it.data(),
           it.size() * sizeof(renderer::GeometryVertexLayout::Data));
    vert += it.size();

    above_offsets_.push_back(vert_add);
    vert_add += it.size();
  }

  // Update z-layers for new buffer
  for (size_t i = 0; i < above_vertices_.size(); ++i) {
    auto& it = above_vertices_[i];
    if (it.empty()) {
      above_layers_[i]->SetIndex(-1);
      above_layers_[i]->SetVisible(false);
      continue;
    }

    above_layers_[i]->SetIndex(i);
    above_layers_[i]->SetVisible(true);
  }
}

void Tilemap::SetupDrawLayersInternal() {
  const DrawableParent::ViewportInfo& viewport_rect =
      ground_layer_->parent_rect();
  UpdateViewportInternal(viewport_rect);

  const base::Vec2i viewport_size = viewport_rect.rect.Size();
  int layer_num =
      (viewport_size.y / tile_size_) + !!(viewport_size.y % tile_size_) + 7;

  for (int i = 0; i < layer_num; ++i)
    above_layers_.push_back(std::make_unique<TilemapZLayer>(
        static_cast<Graphics*>(screen()), this));
}

void Tilemap::ResetZLayerInternal() {
  auto calc_z_order = [&](int index) {
    return 32 * (index + tilemap_viewport_.y + 1) - origin_.y;
  };

  for (int i = 0; i < above_layers_.size(); ++i) {
    std::unique_ptr<TilemapZLayer> new_layer(
        new TilemapZLayer(static_cast<Graphics*>(screen()), this));
    above_layers_[i]->SetZ(calc_z_order(i));
  }
}

void Tilemap::RaiseUpdateAtlasInternal() {
  atlas_need_update_ = true;
}

void Tilemap::RaiseUpdateBufferInternal() {
  map_buffer_need_update_ = true;
}

void Tilemap::DrawFlashLayerInternal(CompositeTargetInfo* target_info) {
  float alpha = kFlashAlpha[flash_alpha_index_] / 255.0f;
  flash_layer_->Composite(target_info, screen(), tilemap_offset_, alpha);
}

void Tilemap::DrawGroundLayerInternal(CompositeTargetInfo* target_info) {
  auto& shader = screen()->renderer()->GetPipelines()->tilemap;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::Normal);
  screen()->renderer()->context()->SetPipelineState(pipeline->pso);

  shader.SetTexture(
      atlas_texture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  screen()->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Tilemap::VSUniform>
        Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                  Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, target_info->viewport_size,
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->texSize = base::MakeInvert(base::Vec2i(
        atlas_texture_->GetDesc().Width, atlas_texture_->GetDesc().Height));
    Constants->transOffset = tilemap_offset_;
    Constants->tileSize = tile_size_;
    Constants->animateIndex = frame_index_;
  }

  int ground_quad_size = ground_vertices_.size() / 4;
  tilemap_quads_->Draw(screen()->renderer()->context(), 0, ground_quad_size);

  DrawFlashLayerInternal(target_info);
}

void Tilemap::DrawAboveLayerInternal(int index,
                                     CompositeTargetInfo* target_info) {
  if (!above_vertices_.size())
    return;

  auto& shader = screen()->renderer()->GetPipelines()->tilemap;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::Normal);
  screen()->renderer()->context()->SetPipelineState(pipeline->pso);

  shader.SetTexture(
      atlas_texture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  screen()->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Tilemap::VSUniform>
        Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                  Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, target_info->viewport_size,
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->texSize = base::MakeInvert(base::Vec2i(
        atlas_texture_->GetDesc().Width, atlas_texture_->GetDesc().Height));
    Constants->transOffset = tilemap_offset_;
    Constants->tileSize = tile_size_;
    Constants->animateIndex = frame_index_;
  }

  int ground_quad_size = ground_vertices_.size() / 4;
  int offset = index ? above_offsets_[index] / 4 : 0;
  tilemap_quads_->Draw(screen()->renderer()->context(),
                       ground_quad_size + offset,
                       above_vertices_[index].size() / 4);
}

}  // namespace content
