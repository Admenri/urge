// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_DRAWABLE_CONTROLLER_H_
#define CONTENT_RENDER_DRAWABLE_CONTROLLER_H_

#include <typeindex>

#include "base/bind/callback.h"
#include "base/containers/linked_list.h"
#include "base/math/rectangle.h"
#include "renderer/device/render_device.h"
#include "renderer/resource/render_buffer.h"

namespace content {

// Using polymorphic memory resources based map for drawable sort,
// the creation of drawable and texture query will cost some allocation time of
// cpu. Consider std::pmr:: domain method.

// DrawController <-> Drawable
// Engine use task-driver renderable structure:
// notify composite -> enumerate children -> called repeatable closure
// Graphics, Viewport hosted the controller, Sprite, Plane, Window, Tilemap...
// inhert drawable.

// A drawable based child render process view:
// <before render> <on render> <after render>
// 1. <before render>:
//   controller parameters:
//     a. renderDevice
//     b. no render target
// 2. <on render>: execute the render command
//   controller parameters:
//     a. renderDevice
//     b. immutable renderPass on <screenBuffer>
//     c. allow screenBuffer texture access
// 3. <after render>: after all render event notification
//     a. renderDevice
//     b. allow screenBuffer texture access

class DrawNodeController;

struct TypeID {
  template <typename Ty>
  static std::size_t Of() {
    static std::byte buffer;
    return reinterpret_cast<std::size_t>(&buffer);
  }
};

// Multi weight sorted key data structure.
struct SortKey {
  int64_t weight[3];

  SortKey();
  SortKey(int64_t key1);
  SortKey(int64_t key1, int64_t key2);
  SortKey(int64_t key1, int64_t key2, int64_t key3);

  // Compare self with other for displaying under other object.
  inline bool operator<(const SortKey& other) const {
    for (int32_t i = 0; i < 3; ++i)
      if (weight[i] != other.weight[i])
        return weight[i] < other.weight[i];

    return false;
  }
};

struct ViewportInfo {
  // Bound in screen coordinate
  base::Rect bound;

  // Origin attribute of current viewport
  base::Vec2i origin;
};

// Stacked scissor state manager
class ScissorStack {
 public:
  ScissorStack(Diligent::IDeviceContext* context, const base::Rect& first);

  ScissorStack(const ScissorStack&) = delete;
  ScissorStack& operator=(const ScissorStack&) = delete;

  // Current storage scissor region
  base::Rect Current();

  // Stack operation for scissor
  bool Push(const base::Rect& scissor);
  void Pop();

  // Reset current scissor state with storage scissor region
  void Reset();

 private:
  void SetScissor(const base::Rect& bound);

  Diligent::IDeviceContext* context_;
  base::Stack<base::Rect> stack_;
};

// Drawable child node,
// expect to be set in class as a node variable.
class DrawableNode final : public base::LinkNode<DrawableNode> {
 public:
  enum RenderStage {
    BEFORE_RENDER = 0,
    ON_RENDERING,
    NOTIFICATION,
  };

  struct RenderControllerParams {
    // [Stage: all]
    // Immediate rendering context.
    Diligent::IDeviceContext* context = nullptr;

    // [Stage: all]
    // Abstract "screen" render buffer.
    Diligent::ITexture* screen_buffer = nullptr;
    Diligent::ITexture* screen_depth_stencil = nullptr;

    // [Stage: all]
    // Screen buffer size.
    base::Vec2i screen_size;

    // [Stage: all]
    // Viewport operation
    ScissorStack* scissors = nullptr;

    // [Stage: on rendering]
    // World transform matrix with offset. (2D)
    Diligent::IBuffer* root_world = nullptr;

    // [Stage: on rendering]
    // Root transform matrix without any offset. (2D)
    Diligent::IBuffer* world_binding = nullptr;
  };

  using NotificationHandler =
      base::RepeatingCallback<void(RenderStage stage,
                                   RenderControllerParams* params)>;

  DrawableNode(DrawNodeController* controller,
               const SortKey& default_key,
               bool visible = true);
  DrawableNode(DrawableNode&& other);
  ~DrawableNode();

  // Register the main executer for current drawable node's host
  void RegisterEventHandler(const NotificationHandler& handler);

  // Rebind parent controller
  void RebindController(DrawNodeController* controller);

  // Dispose drawable node, and close the connection with controller
  void DisposeNode();

  // Set current node visibility
  void SetNodeVisibility(bool visible);
  bool GetVisibility() const;

  // Set the key for requirement of the controller's map sort.
  // For the same weight scene, it can set multi weight value for sort.
  // Current support 3 weights for map sort.
  // No sort action after setting the weight.
  void SetNodeSortWeight(int64_t weight1);
  void SetNodeSortWeight(int64_t weight1, int64_t weight2);
  void SetNodeSortWeight(int64_t weight1, int64_t weight2, int64_t weight3);

  // Get sort key
  SortKey* GetSortKeys() { return &key_; }

  // Get current controller
  DrawNodeController* GetController() const { return controller_; }

  // Batch info setup
  template <typename Ty>
  void SetupBatchable(Ty* self) {
    batch_id_ = TypeID::Of<Ty>();
    batch_self_ = self;
  }

  template <typename Ty>
  Ty* CastToNode() {
    if (batch_id_ == TypeID::Of<Ty>())
      return static_cast<Ty*>(batch_self_);
    return nullptr;
  }

  // Get linked node (previous / next)
  DrawableNode* GetPreviousNode();
  DrawableNode* GetNextNode();

  // Parent viewport
  ViewportInfo* GetParentViewport();

  // Debug info
  void SetDebugLabel(const base::String& label) { debug_label_ = label; }
  base::String GetDebugLabel() const { return debug_label_; }

 private:
  friend class DrawNodeController;

  base::String debug_label_;

  DrawNodeController* controller_;
  NotificationHandler handler_;

  SortKey key_;
  bool visible_;

  std::size_t batch_id_;
  void* batch_self_;
};

// Controller node implement with sorted-map contrainer.
// Can be used nestly in viewport class node.
class DrawNodeController final {
 public:
  DrawNodeController();
  ~DrawNodeController();

  DrawNodeController(const DrawNodeController&) = delete;
  DrawNodeController& operator=(const DrawNodeController&) = delete;

  // Broadcast the notification for all children node,
  // it will raise the handler immediately.
  void BroadCastNotification(DrawableNode::RenderStage nid,
                             DrawableNode::RenderControllerParams* params);

  // Access current viewport info
  inline ViewportInfo& CurrentViewport() { return viewport_; }

 private:
  friend class DrawableNode;

  // Insert child node to controller by sorted key.
  void InsertChildNodeInternal(DrawableNode* node);

  // Nodes sorted by key boardcast list (in-order, Z min to max)
  base::LinkedList<DrawableNode> children_list_;

  // Node cluster attributes
  ViewportInfo viewport_;
};

// Flash duration controller components
class DrawableFlashController {
 public:
  DrawableFlashController() = default;

  DrawableFlashController(const DrawableFlashController&) = delete;
  DrawableFlashController& operator=(const DrawableFlashController&) = delete;

  void Setup(const std::optional<base::Vec4>& flash_color, int32_t duration);
  void Update();

  base::Vec4 GetColor() const { return color_; }
  bool IsFlashing() const { return duration_; }
  bool IsInvalid() const { return invalid_; }

 private:
  int32_t duration_ = 0;
  int32_t count_ = 0;
  base::Vec4 color_;
  float alpha_ = 0.0f;
  bool invalid_ = false;
};

}  // namespace content

#endif  //! CONTENT_RENDER_DRAWABLE_CONTROLLER_H_
