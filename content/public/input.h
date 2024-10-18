// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_INPUT_H_
#define CONTENT_PUBLIC_INPUT_H_

#include <array>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/filesystem/filesystem.h"
#include "content/common/content_utils.h"
#include "content/public/utility.h"
#include "ui/widget/widget.h"

namespace content {

class Input final : public base::RefCounted<Input> {
 public:
  struct KeyBinding {
    std::string sym;
    SDL_Scancode scancode;

    bool operator==(const KeyBinding& other) {
      return sym == other.sym && scancode == other.scancode;
    }
  };

  using KeyState = struct {
    bool pressed;
    bool trigger;
    bool repeat;
    int repeat_count;
  };

  using KeySymMap = std::vector<KeyBinding>;
  Input(base::WeakPtr<ui::Widget> window, content::APIVersion api_diff);
  ~Input() = default;

  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;

  void ApplyKeySymBinding(const KeySymMap& keysyms);

  void Update();

  bool IsPressed(const std::string& keysym);
  bool IsTriggered(const std::string& keysym);
  bool IsRepeated(const std::string& keysym);

  bool KeyPressed(int scancode);
  bool KeyTriggered(int scancode);
  bool KeyRepeated(int scancode);

  std::string GetKeyName(int scancode);
  void GetKeysFromFlag(const std::string& flag, std::vector<int>& out);
  void SetKeysFromFlag(const std::string& flag, const std::vector<int>& keys);

  void GetRecentPressed(std::vector<int>& out);
  void GetRecentTriggered(std::vector<int>& out);
  void GetRecentRepeated(std::vector<int>& out);

  int Dir4();
  int Dir8();

 private:
  void UpdateDir4Internal();
  void UpdateDir8Internal();

  content::APIVersion api_version_;
  KeySymMap key_bindings_;
  std::array<KeyState, SDL_SCANCODE_COUNT> key_states_;
  std::array<KeyState, SDL_SCANCODE_COUNT> recent_key_states_;

  KeySymMap setting_bindings_;

  struct {
    int active = 0;
    int previous = 0;
  } dir4_state_;

  struct {
    int active = 0;
  } dir8_state_;

  base::WeakPtr<ui::Widget> window_;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_INPUT_H_
