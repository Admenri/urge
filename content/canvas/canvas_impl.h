// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_CANVAS_IMPL_H_
#define CONTENT_CANVAS_CANVAS_IMPL_H_

#include <queue>
#include <string>

#include "SDL3/SDL_surface.h"

#include "base/containers/linked_list.h"
#include "content/canvas/font_impl.h"
#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_bitmap.h"
#include "content/render/drawable_controller.h"

namespace content {

constexpr int32_t kBlockMaxSize = 4096;

class CanvasScheduler;

class CanvasImpl : public base::LinkNode<CanvasImpl>,
                   public EngineObject,
                   public Disposable,
                   public Bitmap {
 public:
  struct Agent {
    // Debug name
    base::String name;

    // Bitmap texture data
    base::Vec2i size;
    RRefPtr<Diligent::ITexture> data;
    RRefPtr<Diligent::ITextureView> resource;
    RRefPtr<Diligent::ITextureView> target;

    // Shader binding cache data
    RRefPtr<Diligent::IBuffer> world_buffer;

    // Text drawing cache texture
    base::Vec2i text_cache_size;
    RRefPtr<Diligent::ITexture> text_cache_texture;

    // Filter effect intermediate layer
    RRefPtr<Diligent::ITexture> effect_layer;
  };

  CanvasImpl(ExecutionContext* context,
             SDL_Surface* memory_surface,
             const base::String& debug_name);
  ~CanvasImpl() override;

  CanvasImpl(const CanvasImpl&) = delete;
  CanvasImpl& operator=(const CanvasImpl&) = delete;

  static scoped_refptr<CanvasImpl> Create(ExecutionContext* context,
                                          const base::Vec2i& size,
                                          ExceptionState& exception_state);

  static scoped_refptr<CanvasImpl> Create(ExecutionContext* context,
                                          const base::String& filename,
                                          ExceptionState& exception_state);

  static scoped_refptr<CanvasImpl> FromBitmap(scoped_refptr<Bitmap> host);

  // For debugging usage
  base::String GetCanvasName() const { return name_; }

  // Synchronize pending commands and fetch texture to buffer.
  // Read buffer for surface pixels data.
  SDL_Surface* RequireMemorySurface();
  void InvalidateSurfaceCache();

  // Process queued pending commands.
  void SubmitQueuedCommands();

  // Require render texture (maybe null after disposed)
  Agent* GetAgent() { return Disposable::IsDisposed() ? nullptr : &agent_; }

  // Add a handler for observe bitmap content changing
  inline base::CallbackListSubscription AddCanvasObserver(
      const base::RepeatingClosure& observer) {
    return observers_.Add(observer);
  }

 protected:
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  uint32_t Width(ExceptionState& exception_state) override;
  uint32_t Height(ExceptionState& exception_state) override;
  scoped_refptr<Rect> GetRect(ExceptionState& exception_state) override;
  void Blt(int32_t x,
           int32_t y,
           scoped_refptr<Bitmap> src_bitmap,
           scoped_refptr<Rect> src_rect,
           uint32_t opacity,
           int32_t blend_type,
           ExceptionState& exception_state) override;
  void StretchBlt(scoped_refptr<Rect> dest_rect,
                  scoped_refptr<Bitmap> src_bitmap,
                  scoped_refptr<Rect> src_rect,
                  uint32_t opacity,
                  int32_t blend_type,
                  ExceptionState& exception_state) override;
  void FillRect(int32_t x,
                int32_t y,
                uint32_t width,
                uint32_t height,
                scoped_refptr<Color> color,
                ExceptionState& exception_state) override;
  void FillRect(scoped_refptr<Rect> rect,
                scoped_refptr<Color> color,
                ExceptionState& exception_state) override;
  void GradientFillRect(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        scoped_refptr<Color> color1,
                        scoped_refptr<Color> color2,
                        bool vertical,
                        ExceptionState& exception_state) override;
  void GradientFillRect(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        scoped_refptr<Color> color1,
                        scoped_refptr<Color> color2,
                        ExceptionState& exception_state) override;
  void GradientFillRect(scoped_refptr<Rect> rect,
                        scoped_refptr<Color> color1,
                        scoped_refptr<Color> color2,
                        bool vertical,
                        ExceptionState& exception_state) override;
  void GradientFillRect(scoped_refptr<Rect> rect,
                        scoped_refptr<Color> color1,
                        scoped_refptr<Color> color2,
                        ExceptionState& exception_state) override;
  void Clear(ExceptionState& exception_state) override;
  void ClearRect(int32_t x,
                 int32_t y,
                 uint32_t width,
                 uint32_t height,
                 ExceptionState& exception_state) override;
  void ClearRect(scoped_refptr<Rect> rect,
                 ExceptionState& exception_state) override;
  scoped_refptr<Color> GetPixel(int32_t x,
                                int32_t y,
                                ExceptionState& exception_state) override;
  void SetPixel(int32_t x,
                int32_t y,
                scoped_refptr<Color> color,
                ExceptionState& exception_state) override;
  void HueChange(int32_t hue, ExceptionState& exception_state) override;
  void Blur(ExceptionState& exception_state) override;
  void RadialBlur(int32_t angle,
                  int32_t division,
                  ExceptionState& exception_state) override;
  void DrawText(int32_t x,
                int32_t y,
                uint32_t width,
                uint32_t height,
                const base::String& str,
                int32_t align,
                ExceptionState& exception_state) override;
  void DrawText(int32_t x,
                int32_t y,
                uint32_t width,
                uint32_t height,
                const base::String& str,
                ExceptionState& exception_state) override;
  void DrawText(scoped_refptr<Rect> rect,
                const base::String& str,
                int32_t align,
                ExceptionState& exception_state) override;
  void DrawText(scoped_refptr<Rect> rect,
                const base::String& str,
                ExceptionState& exception_state) override;
  scoped_refptr<Rect> TextSize(const base::String& str,
                               ExceptionState& exception_state) override;

  scoped_refptr<Surface> GetSurface(ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Font, scoped_refptr<Font>);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "Bitmap"; }
  void BlitTextureInternal(const base::Rect& dst_rect,
                           CanvasImpl* src_texture,
                           const base::Rect& src_rect,
                           int32_t blend_type,
                           uint32_t alpha);

  void GPUCreateTextureWithDataInternal();
  void GPUResetEffectLayerIfNeed();
  void GPUBlendBlitTextureInternal(const base::Rect& dst_region,
                                   Agent* src_texture,
                                   const base::Rect& src_region,
                                   int32_t blend_type,
                                   uint32_t blit_alpha);
  void GPUFetchTexturePixelsDataInternal();
  void GPUCanvasClearInternal();
  void GPUCanvasGradientFillRectInternal(const base::Rect& region,
                                         const base::Vec4& color1,
                                         const base::Vec4& color2,
                                         bool vertical);
  void GPUCanvasDrawTextSurfaceInternal(const base::Rect& region,
                                        SDL_Surface* text,
                                        float opacity,
                                        int32_t align);
  void GPUCanvasHueChange(int32_t hue);

  enum class CommandID {
    NONE = 0,
    CLEAR,
    GRADIENT_FILL_RECT,
    HUE_CHANGE,
    RADIAL_BLUR,
    DRAW_TEXT,
  };

  struct Command {
    CommandID id;
    Command* next;

    Command(CommandID cid) : id(cid), next(nullptr) {}
    virtual ~Command() = default;
  };

  struct Command_Clear : public Command {
    Command_Clear() : Command(CommandID::CLEAR) {}
  };

  struct Command_GradientFillRect : public Command {
    base::Rect region;
    base::Vec4 color1;
    base::Vec4 color2;
    bool vertical;

    Command_GradientFillRect() : Command(CommandID::GRADIENT_FILL_RECT) {}
  };

  struct Command_HueChange : public Command {
    int32_t hue;

    Command_HueChange() : Command(CommandID::HUE_CHANGE) {}
  };

  struct Command_RadialBlur : public Command {
    bool blur;
    int32_t angle;
    int32_t division;

    Command_RadialBlur() : Command(CommandID::RADIAL_BLUR) {}
  };

  struct Command_DrawText : public Command {
    base::Rect region;
    SDL_Surface* text;
    float opacity;
    int32_t align;

    Command_DrawText() : Command(CommandID::DRAW_TEXT) {}
  };

  struct CommandBlock {
    uint32_t usage = 0;
    base::Vector<uint8_t> memory;
  };

  template <typename Ty>
  Ty* AllocateCommand() {
    Ty* command = nullptr;
    while (true) {
      // Allocate new block.
      if (current_block_ >= (uint32_t)blocks_.size()) {
        CommandBlock cb;
        cb.memory.assign(kBlockMaxSize, 0);
        blocks_.push_back(std::move(cb));
      }

      // Step into next block
      CommandBlock* block = &blocks_[current_block_];
      size_t left_space = kBlockMaxSize - block->usage;
      if (left_space < sizeof(Ty)) {
        // Allocate new block if no enough memory.
        // Discard remain memory space.
        current_block_++;
        continue;
      }

      // Allocate block and add to the linked list
      void* memory = block->memory.data() + block->usage;
      block->usage += sizeof(Ty);
      command = ::new (memory) Ty;
      command->next = nullptr;
      if (!commands_)
        commands_ = command;
      if (last_command_)
        last_command_->next = command;
      last_command_ = command;

      // Complete command allocation
      break;
    }

    return command;
  }

  inline void ClearPendingCommands() {
    for (auto& it : blocks_) {
      std::memset(it.memory.data(), 0, kBlockMaxSize);
      it.usage = 0;
    }

    last_command_ = nullptr;
    commands_ = nullptr;
    current_block_ = 0;
  }

  Agent agent_;

  base::String name_;
  SDL_Surface* surface_;

  Command* commands_ = nullptr;
  Command* last_command_ = nullptr;
  base::Vector<CommandBlock> blocks_;
  uint32_t current_block_ = 0;

  base::RepeatingClosureList observers_;
  scoped_refptr<FontImpl> font_;
};

}  // namespace content

#endif  //! CONTENT_CANVAS_CANVAS_IMPL_H_
