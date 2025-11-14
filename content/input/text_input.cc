// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/input/text_input.h"

#include "content/common/rect_impl.h"
#include "content/context/execution_context.h"

namespace content {

// static
std::vector<scoped_refptr<TextInputEvent>> TextInputEvent::Update(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  std::vector<EventController::TextInputEventData> raw_events =
      execution_context->event_controller->text_input_events();

  std::vector<scoped_refptr<TextInputEvent>> filtered_events;
  for (auto& it : raw_events)
    filtered_events.push_back(base::MakeRefCounted<TextInputEventImpl>(it));

  return filtered_events;
}

// static
bool TextInputEvent::Enable(ExecutionContext* execution_context,
                            scoped_refptr<Rect> region,
                            ExceptionState& exception_state) {
  auto base_window = execution_context->window;
  auto* host_window = base_window->AsSDLWindow();

  if (region) {
    auto input_area = RectImpl::From(region)->AsSDLRect();
    SDL_SetTextInputArea(host_window, &input_area, 0);
  }

  return SDL_StartTextInput(host_window);
}

// static
bool TextInputEvent::IsActivated(ExecutionContext* execution_context,
                                 ExceptionState& exception_state) {
  auto base_window = execution_context->window;
  auto* host_window = base_window->AsSDLWindow();

  return SDL_TextInputActive(host_window);
}

// static
bool TextInputEvent::Disable(ExecutionContext* execution_context,
                             ExceptionState& exception_state) {
  auto base_window = execution_context->window;
  auto* host_window = base_window->AsSDLWindow();

  bool result = SDL_StopTextInput(host_window);
  SDL_SetTextInputArea(host_window, nullptr, 0);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// TextInputEventImpl Implement

TextInputEventImpl::TextInputEventImpl(
    EventController::TextInputEventData event)
    : event_(event) {}

TextInputEventImpl::~TextInputEventImpl() = default;

TextInputEvent::Type TextInputEventImpl::GetType(
    ExceptionState& exception_state) {
  return event_.type;
}

std::string TextInputEventImpl::GetText(ExceptionState& exception_state) {
  return event_.text;
}

int32_t TextInputEventImpl::GetEditingStart(ExceptionState& exception_state) {
  return event_.select_start;
}

int32_t TextInputEventImpl::GetEditingLength(ExceptionState& exception_state) {
  return event_.select_length;
}

}  // namespace content
