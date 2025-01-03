// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/drawable_controller.h"

namespace content {

// Using global pmr allocator.
// ref: https://zhuanlan.zhihu.com/p/359409607
//
static std::array<char, 1024 * 1024 * 8> array_buffer;
static std::pmr::monotonic_buffer_resource buffer_resource{array_buffer.data(),
                                                           array_buffer.size()};
static std::pmr::unsynchronized_pool_resource pool_resource{
    std::pmr::pool_options{32, 256}, &buffer_resource};

DrawableNode::DrawableNode(const SortKey& default_sort_weight)
    : key_(default_sort_weight),
      node_visibility_(VisibilityState::kVisible),
      controller_(nullptr) {}

DrawableNode::~DrawableNode() {
  DisposeNode();
}

void DrawableNode::RegisterEventHandler(const NotificationHandler& handler) {
  DCHECK(!handler.is_null());
  DCHECK(handler_.is_null());

  handler_ = handler;
}

void DrawableNode::RebindController(DrawNodeController* controller) {
  DCHECK(controller);

  // Bind to new controller and unbind with old controller.
  if (controller_) {
    auto node = controller_->node_.extract(key_);
    DCHECK(!node.empty());

    // Bind to new contrainer
    controller->node_.insert(std::move(node));
  } else {
    // First bind to new controller
    controller_ = controller;
    controller_->node_.emplace(key_, this);
  }
}

void DrawableNode::DisposeNode() {
  DCHECK(controller_);

  controller_->node_.erase(key_);
  controller_ = nullptr;
}

void DrawableNode::SetNodeVisibility(VisibilityState node_state) {
  node_visibility_ = node_state;
}

void DrawableNode::SetNodeSortWeight(int weight1) {
  if (!controller_)
    return;

  auto current_node = controller_->node_.extract(key_);
  if (current_node.empty())
    return;

  std::memset(&key_, 0, sizeof(key_));
  key_.weight[0] = weight1;

  controller_->node_.insert(std::move(current_node));
}

void DrawableNode::SetNodeSortWeight(int weight1, int weight2) {
  if (!controller_)
    return;

  auto current_node = controller_->node_.extract(key_);
  if (current_node.empty())
    return;

  std::memset(&key_, 0, sizeof(key_));
  key_.weight[0] = weight1;
  key_.weight[1] = weight2;

  controller_->node_.insert(std::move(current_node));
}

void DrawableNode::SetNodeSortWeight(int weight1, int weight2, int weight3) {
  if (!controller_)
    return;

  auto current_node = controller_->node_.extract(key_);
  if (current_node.empty())
    return;

  std::memset(&key_, 0, sizeof(key_));
  key_.weight[0] = weight1;
  key_.weight[1] = weight2;
  key_.weight[2] = weight3;

  controller_->node_.insert(std::move(current_node));
}

DrawNodeController::DrawNodeController() : node_(&pool_resource) {}

DrawNodeController::~DrawNodeController() {
  for (auto& it : node_)
    it.second->controller_ = nullptr;
}

void DrawNodeController::BroadCastNotification(
    DrawableNode::RenderStage nid,
    DrawableNode::RenderControllerParams* params) {
  for (auto& it : node_) {
    // Filter notification request
    if (it.second->node_visibility_ ==
        DrawableNode::VisibilityState::kInVisible)
      continue;
    if (it.second->node_visibility_ ==
            DrawableNode::VisibilityState::kNotificationReserved &&
        nid != DrawableNode::RenderStage::kNotification)
      continue;

    // Broadcast render notification
    it.second->handler_.Run(nid, params);
  }
}

}  // namespace content
