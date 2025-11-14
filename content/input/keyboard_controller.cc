// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/input/keyboard_controller.h"

#include "components/filesystem/io_service.h"

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_timer.h"
#include "imgui/imgui.h"

#include "content/context/execution_context.h"
#include "content/profile/command_ids.h"

#define INPUT_CONFIG_SUBFIX ".cfg"

namespace content {

namespace {

const KeyboardControllerImpl::KeyBinding kDefaultKeyboardBindings[] = {
    {"DOWN", SDL_SCANCODE_DOWN},    {"LEFT", SDL_SCANCODE_LEFT},
    {"RIGHT", SDL_SCANCODE_RIGHT},  {"UP", SDL_SCANCODE_UP},

    {"F5", SDL_SCANCODE_F5},        {"F6", SDL_SCANCODE_F6},
    {"F7", SDL_SCANCODE_F7},        {"F8", SDL_SCANCODE_F8},
    {"F9", SDL_SCANCODE_F9},

    {"SHIFT", SDL_SCANCODE_LSHIFT}, {"SHIFT", SDL_SCANCODE_RSHIFT},
    {"CTRL", SDL_SCANCODE_LCTRL},   {"CTRL", SDL_SCANCODE_RCTRL},
    {"ALT", SDL_SCANCODE_LALT},     {"ALT", SDL_SCANCODE_RALT},

    {"A", SDL_SCANCODE_LSHIFT},     {"B", SDL_SCANCODE_ESCAPE},
    {"B", SDL_SCANCODE_KP_0},       {"B", SDL_SCANCODE_X},
    {"C", SDL_SCANCODE_SPACE},      {"C", SDL_SCANCODE_RETURN},
    {"X", SDL_SCANCODE_A},          {"Y", SDL_SCANCODE_S},
    {"Z", SDL_SCANCODE_D},          {"L", SDL_SCANCODE_Q},
    {"R", SDL_SCANCODE_W},
};

const KeyboardControllerImpl::KeyBinding kKeyboardBindings1[] = {
    {"A", SDL_SCANCODE_Z},
    {"C", SDL_SCANCODE_C},
};

const KeyboardControllerImpl::KeyBinding kKeyboardBindings2[] = {
    {"C", SDL_SCANCODE_Z},
};

const std::string kArrowDirsSymbol[] = {
    "DOWN",
    "LEFT",
    "RIGHT",
    "UP",
};

const std::array<std::string, 12> kButtonItems = {
    "A", "B", "C", "X", "Y", "Z", "L", "R", "DOWN", "LEFT", "RIGHT", "UP",
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// KeyboardControllerImpl Implement

KeyboardControllerImpl::KeyboardControllerImpl(
    ExecutionContext* execution_context)
    : EngineObject(execution_context) {
  /* Apply default keyboard bindings */
  for (size_t i = 0; i < std::size(kDefaultKeyboardBindings); ++i)
    key_bindings_.push_back(kDefaultKeyboardBindings[i]);

  if (execution_context->engine_profile->api_version ==
      ContentProfile::APIVersion::RGSS1)
    for (size_t i = 0; i < std::size(kKeyboardBindings1); ++i)
      key_bindings_.push_back(kKeyboardBindings1[i]);

  if (execution_context->engine_profile->api_version >=
      ContentProfile::APIVersion::RGSS2)
    for (size_t i = 0; i < std::size(kKeyboardBindings2); ++i)
      key_bindings_.push_back(kKeyboardBindings2[i]);
}

KeyboardControllerImpl::~KeyboardControllerImpl() = default;

void KeyboardControllerImpl::ProcessEvent(
    const std::optional<EventController::KeyEventData>& event) {
  if (event) {
    raw_states_[event->scancode] = event->is_down;
  } else {
    for (auto& it : raw_states_)
      it = false;
  }
}

void KeyboardControllerImpl::ApplyKeySymBinding(const KeySymMap& keysyms) {
  key_bindings_ = keysyms;
}

void KeyboardControllerImpl::Update(ExceptionState& exception_state) {
  for (int32_t i = 0; i < SDL_SCANCODE_COUNT; ++i) {
    bool key_pressed = raw_states_[static_cast<SDL_Scancode>(i)];

    /* Update key state with elder state */
    key_states_[i].trigger = !key_states_[i].pressed && key_pressed;

    /* After trigger set, set press state */
    key_states_[i].pressed = key_pressed;

    /* Based on press state update the repeat state */
    key_states_[i].repeat = false;
    if (key_states_[i].pressed) {
      ++key_states_[i].repeat_count;

      bool repeated = false;
      // TODO: RGSS 1/2/3 specific process
      repeated = key_states_[i].repeat_count == 1 ||
                 (key_states_[i].repeat_count >= 23 &&
                  (key_states_[i].repeat_count + 1) % 6 == 0);

      key_states_[i].repeat = repeated;
    } else {
      key_states_[i].repeat_count = 0;
    }

    /* Update recent key infos */
    recent_key_states_[i].pressed = key_states_[i].pressed;
    recent_key_states_[i].trigger = key_states_[i].trigger;
    recent_key_states_[i].repeat = key_states_[i].repeat;
  }

  UpdateDir4Internal();
  UpdateDir8Internal();
}

bool KeyboardControllerImpl::IsPressed(const std::string& sym,
                                       ExceptionState& exception_state) {
  if (sym.empty())
    return false;

  for (auto& it : key_bindings_) {
    if (it.sym == sym)
      if (key_states_[it.scancode].pressed)
        return true;
  }

  return false;
}

bool KeyboardControllerImpl::IsTriggered(const std::string& sym,
                                         ExceptionState& exception_state) {
  if (sym.empty())
    return false;

  for (auto& it : key_bindings_) {
    if (it.sym == sym)
      if (key_states_[it.scancode].trigger)
        return true;
  }

  return false;
}

bool KeyboardControllerImpl::IsRepeated(const std::string& sym,
                                        ExceptionState& exception_state) {
  if (sym.empty())
    return false;

  for (auto& it : key_bindings_) {
    if (it.sym == sym)
      if (key_states_[it.scancode].repeat)
        return true;
  }

  return false;
}

int32_t KeyboardControllerImpl::Dir4(ExceptionState& exception_state) {
  return dir4_state_.active;
}

int32_t KeyboardControllerImpl::Dir8(ExceptionState& exception_state) {
  return dir8_state_.active;
}

bool KeyboardControllerImpl::KeyPressed(int32_t scancode,
                                        ExceptionState& exception_state) {
  return key_states_[scancode].pressed;
}

bool KeyboardControllerImpl::KeyTriggered(int32_t scancode,
                                          ExceptionState& exception_state) {
  return key_states_[scancode].trigger;
}

bool KeyboardControllerImpl::KeyRepeated(int32_t scancode,
                                         ExceptionState& exception_state) {
  return key_states_[scancode].repeat;
}

std::string KeyboardControllerImpl::GetKeyName(
    int32_t scancode,
    ExceptionState& exception_state) {
  SDL_Keycode key = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(scancode),
                                           SDL_KMOD_NONE, false);
  return std::string(SDL_GetKeyName(key));
}

std::vector<int32_t> KeyboardControllerImpl::GetKeysFromFlag(
    const std::string& flag,
    ExceptionState& exception_state) {
  std::vector<int32_t> out;
  if (flag.empty())
    return out;

  for (auto& it : key_bindings_)
    if (it.sym == flag)
      out.push_back(it.scancode);

  return out;
}

void KeyboardControllerImpl::SetKeysFromFlag(const std::string& flag,
                                             const std::vector<int32_t>& keys,
                                             ExceptionState& exception_state) {
  if (flag.empty())
    return;

  auto iter = std::remove_if(
      key_bindings_.begin(), key_bindings_.end(),
      [&](const KeyBinding& binding) { return binding.sym == flag; });
  key_bindings_.erase(iter, key_bindings_.end());

  for (const auto& i : keys) {
    KeyBinding binding;
    binding.sym = flag;
    binding.scancode = static_cast<SDL_Scancode>(i);

    key_bindings_.push_back(std::move(binding));
  }
}

std::vector<int32_t> KeyboardControllerImpl::GetRecentPressed(
    ExceptionState& exception_state) {
  std::vector<int32_t> out;

  for (size_t i = 0; i < recent_key_states_.size(); ++i)
    if (recent_key_states_[i].pressed)
      out.push_back(i);

  return out;
}

std::vector<int32_t> KeyboardControllerImpl::GetRecentTriggered(
    ExceptionState& exception_state) {
  std::vector<int32_t> out;

  for (size_t i = 0; i < recent_key_states_.size(); ++i)
    if (recent_key_states_[i].trigger)
      out.push_back(i);

  return out;
}

std::vector<int32_t> KeyboardControllerImpl::GetRecentRepeated(
    ExceptionState& exception_state) {
  std::vector<int32_t> out;

  for (size_t i = 0; i < recent_key_states_.size(); ++i)
    if (recent_key_states_[i].repeat)
      out.push_back(i);

  return out;
}

bool KeyboardControllerImpl::Emulate(int32_t scancode,
                                     bool down,
                                     int32_t modifier,
                                     bool repeat,
                                     ExceptionState& exception_state) {
  SDL_Event emulate_event;
  emulate_event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  emulate_event.key.timestamp = SDL_GetTicksNS();
  emulate_event.key.windowID = context()->window->GetWindowID();
  emulate_event.key.which = 0;
  emulate_event.key.scancode = static_cast<SDL_Scancode>(scancode);
  emulate_event.key.mod = modifier;
  emulate_event.key.raw = 0;
  emulate_event.key.down = down;
  emulate_event.key.repeat = repeat;
  emulate_event.key.key = SDL_GetKeyFromScancode(emulate_event.key.scancode,
                                                 emulate_event.key.mod, true);

  return SDL_PushEvent(&emulate_event);
}

void KeyboardControllerImpl::UpdateDir4Internal() {
  bool key_states[std::size(kArrowDirsSymbol)] = {0};
  for (auto& it : key_bindings_)
    for (size_t i = 0; i < std::size(kArrowDirsSymbol); ++i)
      if (it.sym == kArrowDirsSymbol[i])
        key_states[i] |= key_states_[it.scancode].pressed;

  int32_t dir_flag = 0;
  const int32_t dir_flags_fix[] = {
      1 << 1,
      1 << 2,
      1 << 3,
      1 << 4,
  };

  const int32_t block_dir_flags[] = {dir_flags_fix[0] | dir_flags_fix[3],
                                     dir_flags_fix[1] | dir_flags_fix[2]};

  const int32_t other_dirs[][3] = {
      {1, 2, 3},
      {0, 3, 2},
      {0, 3, 1},
      {1, 2, 0},
  };

  for (size_t i = 0; i < 4; ++i)
    dir_flag |= (key_states[i] ? dir_flags_fix[i] : 0);

  if (dir_flag == block_dir_flags[0] || dir_flag == block_dir_flags[1]) {
    dir4_state_.active = 0;
    return;
  }

  if (dir4_state_.previous) {
    if (key_states[dir4_state_.previous / 2 - 1]) {
      for (size_t i = 0; i < 3; ++i) {
        int32_t other_key = other_dirs[dir4_state_.previous / 2 - 1][i];
        if (!key_states[other_key])
          continue;

        dir4_state_.active = (other_key + 1) * 2;
        return;
      }
    }
  }

  for (size_t i = 0; i < 4; ++i) {
    if (!key_states[i])
      continue;

    dir4_state_.active = (i + 1) * 2;
    dir4_state_.previous = (i + 1) * 2;
    return;
  }

  dir4_state_.active = 0;
  dir4_state_.previous = 0;
}

void KeyboardControllerImpl::UpdateDir8Internal() {
  bool key_states[std::size(kArrowDirsSymbol)] = {0};
  for (auto& it : key_bindings_)
    for (size_t i = 0; i < std::size(kArrowDirsSymbol); ++i)
      if (it.sym == kArrowDirsSymbol[i])
        key_states[i] |= key_states_[it.scancode].pressed;

  static const int32_t combos[4][4] = {
      {2, 1, 3, 0}, {1, 4, 0, 7}, {3, 0, 6, 9}, {0, 7, 9, 8}};

  const int32_t other_dirs[][3] = {
      {1, 2, 3},
      {0, 3, 2},
      {0, 3, 1},
      {1, 2, 0},
  };

  dir8_state_.active = 0;

  for (size_t i = 0; i < 4; ++i) {
    if (!key_states[i])
      continue;

    for (int32_t j = 0; j < 3; ++j) {
      int32_t other_key = other_dirs[i][j];
      if (!key_states[other_key])
        continue;

      dir8_state_.active = combos[i][other_key];
      return;
    }

    dir8_state_.active = (i + 1) * 2;
    return;
  }
}

void KeyboardControllerImpl::TryReadBindingsInternal() {
  std::string filepath = context()->engine_profile->program_name;
  filepath += INPUT_CONFIG_SUBFIX;
  filepath += std::to_string(
      static_cast<int32_t>(context()->engine_profile->api_version));

  LOG(INFO) << "[Keyboard] Reading key binding table...";

  filesystem::IOState io_state;
  auto* fstream = context()->io_service->OpenReadRaw(filepath, &io_state);
  if (fstream) {
    key_bindings_.clear();

    Uint32 item_size;
    SDL_ReadIO(fstream, &item_size, sizeof(item_size));
    for (uint32_t i = 0; i < item_size; ++i) {
      Uint32 token_size;
      SDL_ReadIO(fstream, &token_size, sizeof(token_size));

      std::string token(token_size, 0);
      SDL_ReadIO(fstream, token.data(), token_size);

      SDL_Scancode scancode;
      SDL_ReadIO(fstream, &scancode, sizeof(scancode));

      key_bindings_.push_back({token, scancode});
    }

    SDL_CloseIO(fstream);
  }
}

void KeyboardControllerImpl::StorageBindingsInternal() {
  std::string filepath = context()->engine_profile->program_name;
  filepath += INPUT_CONFIG_SUBFIX;
  filepath += std::to_string(
      static_cast<int32_t>(context()->engine_profile->api_version));

  LOG(INFO) << "[Keyboard] Saving key binding table...";

  filesystem::IOState io_state;
  auto* fstream = context()->io_service->OpenWrite(filepath, &io_state);
  if (fstream) {
    Uint32 item_size = key_bindings_.size();
    SDL_WriteIO(fstream, &item_size, sizeof(item_size));
    for (const auto& it : key_bindings_) {
      uint32_t token_size = it.sym.size();
      SDL_WriteIO(fstream, &token_size, sizeof(token_size));

      SDL_WriteIO(fstream, it.sym.data(), token_size);

      SDL_Scancode scancode = it.scancode;
      SDL_WriteIO(fstream, &scancode, sizeof(scancode));
    }

    SDL_CloseIO(fstream);
  }
}

}  // namespace content
