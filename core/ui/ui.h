// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once
#include <deque>
#include <map>

#include "SDL3/SDL_events.h"
#include "core/ui/ui_event.h"
#include "core/ui/ui_raycaster.h"
namespace urge {
class UICanvas;
URGE_BINDING()
class UI : public Singleton<UI> {
 public:
  UI() = default;
  ~UI();
  URGE_BINDING()
  void Update();
  URGE_BINDING()
  RefPtr<UIEvent> PollEvent();
  URGE_BINDING()
  void Focus(MaybePtr<Node> node);
  URGE_BINDING()
  RefPtr<Node> FocusedNode() const { return focused_; }
  URGE_BINDING()
  void CancelPointers();
  URGE_BINDING()
  bool Pointer(std::string type, float x, float y, int64_t pointer_id = 0);
  URGE_BINDING()
  bool Key(int32_t scancode, bool shift = false, bool control = false);
  URGE_BINDING()
  bool Text(std::string text);
  URGE_BINDING()
  bool Scroll(float x, float y, float delta_x, float delta_y);
  bool ProcessEvent(const SDL_Event& event);
  void RegisterCanvas(UICanvas* canvas);
  void UnregisterCanvas(UICanvas* canvas);
  void Forget(Node* node);
  void PrepareTree(Node* root, const glm::vec2& target_size);
  bool KeyboardBlocked() const;
  bool PointerBlocked() const;
  void Emit(std::string type, Node* target, int64_t pointer = -1,
            glm::vec2 position = glm::vec2(0), glm::vec2 local = glm::vec2(0));

 private:
  struct Capture {
    RefPtr<Node> target, drag_target;
    glm::vec2 start{0}, last{0};
    bool dragged = false;
    float slider_offset = 0;
  };
  std::vector<UICanvas*> canvases_;
  std::deque<RefPtr<UIEvent>> events_;
  std::map<int64_t, Capture> captures_;
  std::map<std::pair<int64_t, int64_t>, int64_t> touch_ids_;
  int64_t next_touch_id_ = 1;
  RefPtr<Node> hovered_, focused_;
  RefPtr<UIRaycastHit> Hit(glm::vec2 point);
  Node* Modal() const;
  void Activate(Node* target, glm::vec2 local);
  void SetSlider(Node* target, glm::vec2 point, float offset = 0);
  void MoveFocus(int direction);
  bool ReplaceSelection(std::string text);
};
}  // namespace urge
