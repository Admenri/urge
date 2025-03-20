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

// Multi weight sorted key data structure.
struct SortKey {
  int64_t weight[3];

  SortKey();
  SortKey(int64_t key1);
  SortKey(int64_t key1, int64_t key2);
  SortKey(int64_t key1, int64_t key2, int64_t key3);

  // Compare self with other for displaying under other object.
  inline bool operator<(const SortKey& other) const {
    for (int i = 0; i < 3; ++i)
      if (weight[i] != other.weight[i])
        return weight[i] < other.weight[i];

    return false;
  }
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
    // Logic abstract render device for drawable node.
    // Never be null whenever events.
    renderer::RenderDevice* device = nullptr;

    // [Stage: all]
    // Hardware render command encoder,
    // handler: writeBuffer, writeTexture, copyTexture
    wgpu::CommandEncoder* command_encoder = nullptr;

    // [Stage: all]
    // Abstract "screen" render buffer,
    // maybe graphics or viewport snapshot buffer.
    wgpu::Texture* screen_buffer = nullptr;
    base::Vec2i screen_size;

    // [Stage: all]
    // Current viewport region, origin offset.
    base::Rect viewport;
    base::Vec2i origin;

    // [Stage: on rendering]
    // Main render pass encoder.
    wgpu::RenderPassEncoder* renderpass_encoder = nullptr;

    // [Stage: on rendering]
    // World transform matrix.
    wgpu::BindGroup* root_world = nullptr;
    wgpu::BindGroup* world_binding = nullptr;
  };

  using NotificationHandler =
      base::RepeatingCallback<void(RenderStage stage,
                                   RenderControllerParams* params)>;

  DrawableNode(DrawNodeController* controller,
               const SortKey& default_key,
               bool visible = true);
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
  void SetNodeSortWeight(int weight1);
  void SetNodeSortWeight(int weight1, int weight2);
  void SetNodeSortWeight(int weight1, int weight2, int weight3);

  // Get sort key
  SortKey* GetSortKeys() { return &key_; }

  // Get current controller
  DrawNodeController* GetController() const { return controller_; }

  // Batch info setup
  template <typename Ty>
  void SetupBatchable(Ty* self) {
    batch_info_ = typeid(Ty);
    batch_node_ = self;
  }

  template <typename Ty>
  Ty* CastToNode() {
    if (batch_info_ == typeid(Ty))
      return static_cast<Ty*>(batch_self_);
    return nullptr;
  }

  // Get linked node (previous / next)
  DrawableNode* GetPreviousNode();
  DrawableNode* GetNextNode();

 private:
  friend class DrawNodeController;

  DrawNodeController* controller_;
  NotificationHandler handler_;

  SortKey key_;
  bool visible_;

  std::type_index batch_info_;
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

 private:
  friend class DrawableNode;

  // Insert child node to controller by sorted key.
  void InsertChildNodeInternal(DrawableNode* node);

  // Nodes sorted by key boardcast list (in-order, Z min to max)
  base::LinkedList<DrawableNode> children_list_;
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
