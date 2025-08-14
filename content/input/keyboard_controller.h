// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_INPUT_KEYBOARD_CONTROLLER_H_
#define CONTENT_INPUT_KEYBOARD_CONTROLLER_H_

#include "content/context/engine_object.h"
#include "content/profile/content_profile.h"
#include "content/profile/i18n_profile.h"
#include "content/public/engine_input.h"
#include "ui/widget/widget.h"

namespace content {

class KeyboardControllerImpl : public Input, public EngineObject {
 public:
  struct KeyBinding {
    std::string sym;
    SDL_Scancode scancode;

    bool operator==(const KeyBinding& other) const {
      return sym == other.sym && scancode == other.scancode;
    }
  };

  using KeySymMap = std::vector<KeyBinding>;
  using KeyState = struct {
    bool pressed;
    bool trigger;
    bool repeat;
    int32_t repeat_count;
  };

  KeyboardControllerImpl(ExecutionContext* execution_context);
  ~KeyboardControllerImpl() override;

  KeyboardControllerImpl(const KeyboardControllerImpl&) = delete;
  KeyboardControllerImpl& operator=(const KeyboardControllerImpl&) = delete;

  void ApplyKeySymBinding(const KeySymMap& keysyms);
  bool CreateButtonGUISettings();

 public:
  void Update(ExceptionState& exception_state) override;
  bool IsPressed(const std::string& sym,
                 ExceptionState& exception_state) override;
  bool IsTriggered(const std::string& sym,
                   ExceptionState& exception_state) override;
  bool IsRepeated(const std::string& sym,
                  ExceptionState& exception_state) override;
  int32_t Dir4(ExceptionState& exception_state) override;
  int32_t Dir8(ExceptionState& exception_state) override;

  bool KeyPressed(int32_t scancode, ExceptionState& exception_state) override;
  bool KeyTriggered(int32_t scancode, ExceptionState& exception_state) override;
  bool KeyRepeated(int32_t scancode, ExceptionState& exception_state) override;
  std::string GetKeyName(int32_t scancode,
                         ExceptionState& exception_state) override;
  std::vector<int32_t> GetKeysFromFlag(
      const std::string& flag,
      ExceptionState& exception_state) override;
  void SetKeysFromFlag(const std::string& flag,
                       const std::vector<int32_t>& keys,
                       ExceptionState& exception_state) override;

  std::vector<int32_t> GetRecentPressed(
      ExceptionState& exception_state) override;
  std::vector<int32_t> GetRecentTriggered(
      ExceptionState& exception_state) override;
  std::vector<int32_t> GetRecentRepeated(
      ExceptionState& exception_state) override;

  bool Emulate(int32_t scancode,
               bool down,
               int32_t modifier,
               bool repeat,
               ExceptionState& exception_state) override;

 private:
  void UpdateDir4Internal();
  void UpdateDir8Internal();

  void TryReadBindingsInternal();
  void StorageBindingsInternal();

  KeySymMap key_bindings_;
  KeySymMap setting_bindings_;
  bool disable_gui_key_input_;

  std::array<KeyState, SDL_SCANCODE_COUNT> key_states_;
  std::array<KeyState, SDL_SCANCODE_COUNT> recent_key_states_;

  struct {
    int32_t active = 0;
    int32_t previous = 0;
  } dir4_state_;

  struct {
    int32_t active = 0;
  } dir8_state_;
};

}  // namespace content

#endif  //! CONTENT_INPUT_CONTROLLER_KEYBOARD_H_
