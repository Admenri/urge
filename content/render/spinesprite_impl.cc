// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/spinesprite_impl.h"

namespace content {

scoped_refptr<SpineSprite> SpineSprite::New(
    ExecutionContext* execution_context,
    const std::string& atlas_filename,
    const std::string& skeleton_filename,
    float default_mix,
    ExceptionState& exception_state) {
  auto* io_service = execution_context->io_service;

  // Read necessary data from filesystem
  filesystem::IOState io_state;
  auto* atlas_stream = io_service->OpenReadRaw(atlas_filename, &io_state);
  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               io_state.error_message.c_str());
    return nullptr;
  }

  auto* skeleton_stream = io_service->OpenReadRaw(skeleton_filename, &io_state);
  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               io_state.error_message.c_str());
    return nullptr;
  }

  // Read all data in memory
  int64_t atlas_size = SDL_GetIOSize(atlas_stream);
  std::string atlas_data(atlas_size, 0);
  SDL_ReadIO(atlas_stream, atlas_data.data(), atlas_size);

  int64_t skeleton_size = SDL_GetIOSize(skeleton_stream);
  std::string skeleton_data(skeleton_size, 0);
  SDL_ReadIO(skeleton_stream, skeleton_data.data(), skeleton_size);

  // Get directory from atlas path
  const auto* atlas_path = atlas_filename.c_str();
  const char* last_forward_slash = std::strrchr(atlas_path, '/');
  const char* last_backward_slash = std::strrchr(atlas_path, '\\');
  const char* last_slash = last_forward_slash > last_backward_slash
                               ? last_forward_slash
                               : last_backward_slash;
  if (last_slash == atlas_path)
    last_slash++; /* Never drop starting slash. */
  int32_t dir_length =
      static_cast<int32_t>(last_slash ? last_slash - atlas_path : 0);
  std::string dir(dir_length + 1, 0);
  std::memcpy(dir.data(), atlas_path, dir_length);
  dir[dir_length] = '\0';

  // Loading spine componnets
  spine::Bone::setYDown(true);
  std::unique_ptr<spine::DiligentTextureLoader> texture_loader =
      std::make_unique<spine::DiligentTextureLoader>(
          execution_context->render_device, io_service);
  std::unique_ptr<spine::Atlas> atlas =
      std::make_unique<spine::Atlas>(atlas_data.data(), atlas_data.size(),
                                     dir.c_str(), texture_loader.get(), true);

  std::string skeleton_loading_error;
  spine::SkeletonData* skeleton_data_ptr = nullptr;
  if (skeleton_filename.substr(skeleton_filename.size() - 4) == "json") {
    spine::SkeletonJson json_loader(atlas.get());
    skeleton_data_ptr = json_loader.readSkeletonData(skeleton_data.c_str());
    if (!json_loader.getError().isEmpty())
      skeleton_loading_error = json_loader.getError().buffer();
  } else {
    spine::SkeletonBinary binary_loader(atlas.get());
    skeleton_data_ptr = binary_loader.readSkeletonData(
        reinterpret_cast<uint8_t*>(skeleton_data.data()), skeleton_data.size());
    if (!binary_loader.getError().isEmpty())
      skeleton_loading_error = binary_loader.getError().buffer();
  }

  if (!skeleton_data_ptr) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               skeleton_loading_error.c_str());
    return nullptr;
  }

  std::unique_ptr<spine::AnimationStateData> animation_state_data =
      std::make_unique<spine::AnimationStateData>(skeleton_data_ptr);
  animation_state_data->setDefaultMix(default_mix);

  return base::MakeRefCounted<SpineSpriteImpl>(
      execution_context, std::move(atlas), std::move(texture_loader),
      std::unique_ptr<spine::SkeletonData>(skeleton_data_ptr),
      std::move(animation_state_data));
}

SpineEventImpl::SpineEventImpl(spine::EventType type,
                               spine::Event* event,
                               spine::TrackEntry* track_entry)
    : type_(type),
      name_(event ? event->getData().getName().buffer() : ""),
      track_index_(track_entry->getTrackIndex()),
      time_(event ? event->getTime() : 0.0f),
      int_value_(event ? event->getIntValue() : 0),
      float_value_(event ? event->getFloatValue() : 0.0f),
      string_value_(event ? event->getStringValue().buffer() : ""),
      volume_(event ? event->getVolume() : 0.0f),
      balance_(event ? event->getBalance() : 0.0f) {}

SpineEventImpl::~SpineEventImpl() = default;

SpineEvent::Type SpineEventImpl::GetType(ExceptionState& exception_state) {
  return static_cast<SpineEvent::Type>(type_);
}

std::string SpineEventImpl::GetName(ExceptionState& exception_state) {
  return name_;
}

int32_t SpineEventImpl::GetTrackIndex(ExceptionState& exception_state) {
  return track_index_;
}

float SpineEventImpl::GetTime(ExceptionState& exception_state) {
  return time_;
}

int32_t SpineEventImpl::GetIntValue(ExceptionState& exception_state) {
  return int_value_;
}

float SpineEventImpl::GetFloatValue(ExceptionState& exception_state) {
  return float_value_;
}

std::string SpineEventImpl::GetStringValue(ExceptionState& exception_state) {
  return string_value_;
}

float SpineEventImpl::GetVolume(ExceptionState& exception_state) {
  return volume_;
}

float SpineEventImpl::GetBalance(ExceptionState& exception_state) {
  return balance_;
}

SpineSpriteImpl::SpineSpriteImpl(
    ExecutionContext* execution_context,
    std::unique_ptr<spine::Atlas> atlas,
    std::unique_ptr<spine::DiligentTextureLoader> texture_loader,
    std::unique_ptr<spine::SkeletonData> skeleton_data,
    std::unique_ptr<spine::AnimationStateData> animation_state_data)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      node_(execution_context->screen_drawable_node, SortKey()),
      atlas_(std::move(atlas)),
      texture_loader_(std::move(texture_loader)),
      skeleton_data_(std::move(skeleton_data)),
      animation_state_data_(std::move(animation_state_data)),
      last_ticks_(SDL_GetPerformanceCounter()),
      premultiplied_alpha_(false) {
  node_.RegisterEventHandler(base::BindRepeating(
      &SpineSpriteImpl::DrawableNodeHandlerInternal, base::Unretained(this)));

  skeleton_ = std::make_unique<spine::Skeleton>(skeleton_data_.get());
  animation_state_ =
      std::make_unique<spine::AnimationState>(animation_state_data_.get());
  animation_state_->setListener(this);

  renderer_ = std::make_unique<spine::DiligentRenderer>(
      execution_context->render_device);
}

DISPOSABLE_DEFINITION(SpineSpriteImpl);

void SpineSpriteImpl::SetLabel(const std::string& label,
                               ExceptionState& exception_state) {
  node_.SetDebugLabel(label);
}

std::vector<scoped_refptr<SpineEvent>> SpineSpriteImpl::Update(
    ExceptionState& exception_state) {
  // Update delta time
  uint64_t now = SDL_GetPerformanceCounter();
  float delta_time =
      (now - last_ticks_) / static_cast<float>(SDL_GetPerformanceFrequency());
  last_ticks_ = now;

  // Clear previous queued event
  queued_event_.clear();

  // Update animation state and events
  animation_state_->update(delta_time);
  animation_state_->apply(*skeleton_);
  skeleton_->update(delta_time);
  skeleton_->updateWorldTransform(spine::Physics_Update);

  return queued_event_;
}

void SpineSpriteImpl::SetAnimation(int32_t track_index,
                                   const std::string& name,
                                   bool loop,
                                   ExceptionState& exception_state) {
  if (!animation_state_->setAnimation(track_index, name.c_str(), loop))
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "failed to find track entry: %d", track_index);
}

void SpineSpriteImpl::SetAnimationAlpha(int32_t track_index,
                                        float alpha,
                                        ExceptionState& exception_state) {
  auto& tracks = animation_state_->getTracks();

  if (track_index < 0 || track_index >= static_cast<int32_t>(tracks.size()))
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid track index: %d", track_index);

  tracks[track_index]->setAlpha(alpha);
}

void SpineSpriteImpl::ClearAnimation(int32_t track_index,
                                     ExceptionState& exception_state) {
  animation_state_->clearTrack(track_index);
}

void SpineSpriteImpl::SetSkin(const std::vector<std::string>& skin_array,
                              ExceptionState& exception_state) {
  auto new_skin_set = std::make_unique<spine::Skin>("new_skin");

  for (const auto& skin_name : skin_array) {
    auto* skin = skeleton_data_->findSkin(skin_name.c_str());
    if (!skin)
      return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                        "failed to find skin: %s",
                                        skin_name.c_str());

    new_skin_set->addSkin(skin);
  }

  skeleton_->setSkin(new_skin_set.get());
  skin_ = std::move(new_skin_set);
}

void SpineSpriteImpl::SetBonePosition(const std::string& bone_name,
                                      float x,
                                      float y,
                                      ExceptionState& exception_state) {
  auto* bone = skeleton_->findBone(bone_name.c_str());
  if (!bone)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "failed to find bone: %s",
                                      bone_name.c_str());

  bone->setX(x);
  bone->setY(-y);
}

scoped_refptr<Viewport> SpineSpriteImpl::Get_Viewport(
    ExceptionState& exception_state) {
  return viewport_;
}

void SpineSpriteImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                   : context()->screen_drawable_node);
}

bool SpineSpriteImpl::Get_Visible(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return node_.GetVisibility();
}

void SpineSpriteImpl::Put_Visible(const bool& value,
                                  ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeVisibility(value);
}

float SpineSpriteImpl::Get_X(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return skeleton_->getX();
}

void SpineSpriteImpl::Put_X(const float& value,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  skeleton_->setX(value);
}

float SpineSpriteImpl::Get_Y(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return skeleton_->getY();
}

void SpineSpriteImpl::Put_Y(const float& value,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  skeleton_->setY(value);
}

int32_t SpineSpriteImpl::Get_Z(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return node_.GetSortKeys()->weight[0];
}

void SpineSpriteImpl::Put_Z(const int32_t& value,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeSortWeight(value);
}

float SpineSpriteImpl::Get_ZoomX(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return skeleton_->getScaleX();
}

void SpineSpriteImpl::Put_ZoomX(const float& value,
                                ExceptionState& exception_state) {
  DISPOSE_CHECK;

  skeleton_->setScaleX(value);
}

float SpineSpriteImpl::Get_ZoomY(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return skeleton_->getScaleY();
}

void SpineSpriteImpl::Put_ZoomY(const float& value,
                                ExceptionState& exception_state) {
  DISPOSE_CHECK;

  skeleton_->setScaleY(value);
}

bool SpineSpriteImpl::Get_PremultipliedAlpha(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return premultiplied_alpha_;
}

void SpineSpriteImpl::Put_PremultipliedAlpha(const bool& value,
                                             ExceptionState& exception_state) {
  DISPOSE_CHECK;

  premultiplied_alpha_ = value;
}

void SpineSpriteImpl::OnObjectDisposed() {
  node_.DisposeNode();

  renderer_.reset();
  animation_state_.reset();
  skeleton_.reset();
  animation_state_data_.reset();
  skeleton_data_.reset();
  atlas_.reset();
  texture_loader_.reset();
}

void SpineSpriteImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    renderer_->Update(params->context, skeleton_.get());
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    renderer_->Render(params->context, params->world_binding,
                      premultiplied_alpha_);
  }
}

void SpineSpriteImpl::callback(spine::AnimationState* state,
                               spine::EventType type,
                               spine::TrackEntry* entry,
                               spine::Event* event) {
  queued_event_.push_back(
      base::MakeRefCounted<SpineEventImpl>(type, event, entry));
}

}  // namespace content
