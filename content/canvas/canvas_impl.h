// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_CANVAS_IMPL_H_
#define CONTENT_CANVAS_CANVAS_IMPL_H_

#include "SDL3/SDL_surface.h"

#include "base/containers/linked_list.h"
#include "content/public/engine_bitmap.h"
#include "content/render/drawable_controller.h"
#include "renderer/context/device_context.h"

#include <queue>

namespace content {

class CanvasScheduler;

constexpr int32_t kBlockMaxSize = 4096;

// Pooling object texture agent,
// used for async thread task runner.
struct TextureAgent {
  wgpu::Texture data;

  static TextureAgent* Allocate(size_t n = 1);
  static void Free(TextureAgent* ptr, size_t n = 1);
};

class CanvasImpl : public Bitmap, public base::LinkNode<CanvasImpl> {
 public:
  CanvasImpl(wgpu::Texture canvas_texture, CanvasScheduler* scheduler);
  ~CanvasImpl() override;

  CanvasImpl(const CanvasImpl&) = delete;
  CanvasImpl& operator=(const CanvasImpl&) = delete;

  // Synchronize pending commands and fetch texture to buffer.
  // Read buffer for surface pixels data.
  SDL_Surface* RequireMemorySurface();

  // Update memory surface to GPU.
  void UpdateVideoMemory();

  // Process queued pending commands.
  void SubmitQueuedCommands(const wgpu::CommandEncoder& encoder);

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
           ExceptionState& exception_state) override;
  void StretchBlt(scoped_refptr<Rect> dest_rect,
                  scoped_refptr<Bitmap> src_bitmap,
                  scoped_refptr<Rect> src_rect,
                  uint32_t opacity,
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
  void GradientFillRect(scoped_refptr<Rect> rect,
                        scoped_refptr<Color> color1,
                        scoped_refptr<Color> color2,
                        bool vertical,
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
                const std::string& str,
                int32_t align,
                ExceptionState& exception_state) override;
  void DrawText(scoped_refptr<Rect> rect,
                const std::string& str,
                int32_t align,
                ExceptionState& exception_state) override;
  scoped_refptr<Rect> TextSize(const std::string& str,
                               ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Font, scoped_refptr<Font>);

 private:
  bool CheckDisposed(ExceptionState& exception_state);
  void BlitTextureInternal(const base::Rect& dst_rect,
                           const wgpu::Texture& src_texture,
                           const base::Rect& src_rect);

  enum class CommandID {
    kFillRect = 0,
    kGradientFillRect,
    kHueChange,
    kBlur,
    kRadialBlur,
    kDrawText,
  };

  struct Command {
    CommandID id;
    Command* next = nullptr;

    virtual ~Command() = default;
  };

  struct Command_FillRect : public Command {
    base::Rect region;
    base::Vec4 color;

    Command_FillRect() { id = CommandID::kFillRect; }
  };

  struct Command_GradientFillRect : public Command {
    base::Rect region;
    base::Vec4 color1;
    base::Vec4 color2;
    bool vertical;

    Command_GradientFillRect() { id = CommandID::kGradientFillRect; }
  };

  struct Command_HueChange : public Command {
    int32_t hue;

    Command_HueChange() { id = CommandID::kHueChange; }
  };

  struct Command_RadialBlur : public Command {
    bool blur;
    int32_t angle;
    int32_t division;

    Command_RadialBlur() { id = CommandID::kRadialBlur; }
  };

  struct Command_DrawText : public Command {
    base::Rect region;
    SDL_Surface* text;
    int align;

    Command_DrawText() { id = CommandID::kDrawText; }
  };

  struct CommandBlock {
    uint32_t usage = 0;
    uint8_t* memory = nullptr;

    ~CommandBlock() {
      if (memory)
        std::free(memory);
    }
  };

  template <typename Ty>
  Ty* AllocateCommand() {
    Ty* command = nullptr;
    while (true) {
      // Allocate new block.
      if (current_block_ == (uint32_t)blocks_.size()) {
        CommandBlock cb;
        cb.memory = (uint8_t*)std::malloc(kBlockMaxSize);
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
      void* memory = block->memory + block->usage;
      block->usage += sizeof(Ty);
      command = ::new (memory) Ty;
      command->next = nullptr;
      if (!commands_)
        commands_ = command;
      if (last_command_)
        last_command_->next = command;
      last_command_ = command;

      // Complete command alllocation
      break;
    }

    return command;
  }

  inline void ClearPendingCommands() {
    Command* current = commands_;
    while (current) {
      current->~Command();
      current = current->next;
    }

    for (auto& it : blocks_)
      it.usage = 0;

    last_command_ = nullptr;
    commands_ = nullptr;
    current_block_ = 0;
  }

  wgpu::Texture canvas_texture_;

  Command* commands_ = nullptr;
  Command* last_command_ = nullptr;
  std::vector<CommandBlock> blocks_;
  uint32_t current_block_ = 0;

  CanvasScheduler* scheduler_;

  scoped_refptr<Font> font_;
};

}  // namespace content

#endif  //! CONTENT_CANVAS_CANVAS_IMPL_H_
