// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/system/config.h"
#include <charconv>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_platform_defines.h"
#include "SDL3/SDL_stdinc.h"

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/input/gamepad.h"
#include "core/system/filesystem.h"
#include "core/system/ini_encoding.h"

namespace urge {

namespace {

static void ReplaceStringWidth(std::string& str, char before, char after) {
  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] == before)
      str[i] = after;
}

std::vector<std::string> SplitKeys(const std::string& line) {
  std::vector<std::string> names;
  size_t start = 0;
  for (;;) {
    const size_t comma = line.find(',', start);
    const std::string name = line.substr(
        start, comma == std::string::npos ? std::string::npos : comma - start);
    if (!name.empty())
      names.push_back(name);
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  return names;
}

WindowResizable ParseResizable(const std::string& value) {
  if (SDL_strcasecmp(value.c_str(), "free") == 0)
    return WindowResizable::kFree;
  if (SDL_strcasecmp(value.c_str(), "ratio") == 0)
    return WindowResizable::kRatio;
  if (!value.empty() && SDL_strcasecmp(value.c_str(), "none") != 0)
    LOGGER_WARN(
        "[Window] Resizable='{}' is not free/ratio/none; the window "
        "stays fixed",
        value);
  return WindowResizable::kNone;
}

std::string ResizableName(WindowResizable value) {
  switch (value) {
    case WindowResizable::kFree:
      return "free";
    case WindowResizable::kRatio:
      return "ratio";
    case WindowResizable::kNone:
      break;
  }
  return "none";
}

int32_t ParseKeyName(const std::string& name) {
  const auto scancode = SDL_GetScancodeFromName(name.c_str());
  if (scancode != SDL_SCANCODE_UNKNOWN)
    return static_cast<int32_t>(scancode);
  int32_t value = static_cast<int32_t>(SDL_SCANCODE_UNKNOWN);
  const auto parsed =
      std::from_chars(name.data(), name.data() + name.size(), value);
  if (parsed.ec != std::errc() || parsed.ptr != name.data() + name.size())
    return static_cast<int32_t>(SDL_SCANCODE_UNKNOWN);
  return value;
}

std::string KeyNameOf(int32_t keycode) {
  const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(keycode));
  return (name && *name) ? std::string(name) : std::to_string(keycode);
}

std::string KeyNameOf(const urge::KeySym& binding) {
  if (binding.gamepad)
    return "pad:" + GamepadDevice::ButtonToken(binding.code);

  return KeyNameOf(binding.code);
}

urge::KeySym ParseKeyBinding(const std::string& symbol,
                             const std::string& value) {
  KeySym binding;
  binding.name = symbol;

  constexpr const char kPadPrefix[] = "pad:";
  constexpr size_t kPadPrefixLength = sizeof(kPadPrefix) - 1;

  if (value.compare(0, kPadPrefixLength, kPadPrefix) == 0) {
    binding.gamepad = true;
    if (!GamepadDevice::ButtonFromToken(value.substr(kPadPrefixLength),
                                        &binding.code))
      binding.code = -1;
    return binding;
  }

  binding.code = ParseKeyName(value);
  return binding;
}

int32_t ParseInteger(const std::string& text, int32_t fallback) {
  int32_t value = fallback;
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
    return fallback;
  return value;
}

bool ParseBoolean(const std::string& text, bool fallback) {
  if (text.empty())
    return fallback;

  const auto equal_ignore_case = [](const std::string& left,
                                    const char* right) {
    const size_t length = SDL_strlen(right);
    return left.size() == length &&
           SDL_strncasecmp(left.c_str(), right, length) == 0;
  };
  if (equal_ignore_case(text, "true") || text == "1" ||
      equal_ignore_case(text, "yes"))
    return true;
  return false;
}

std::string BooleanText(bool value) {
  return value ? "true" : "false";
}

enum class ConfigPersist {
  // Written back when the value moved away from what Load() produced.
  kOnChange,
  // Never written by Save(): the file keeps the text the game author wrote.
  kNever,
};

// One entry of the ini <-> memory map.  Load() applies the entries in order,
// so a field that computes a default can rely on everything above it; Save()
// compares against the snapshot taken after loading rather than writing the
// whole struct back.
struct ConfigField {
  const char* section;
  const char* key;
  ConfigPersist persist;
  std::string (*get)(const Config&);
  void (*set)(Config&, const std::string&);
  void (*after_load)(Config&);
  std::string (*fallback)(const Config&);
};

void DeriveRGSSVersion(Config& config) {
  if (config.game.rgss != 0)
    return;

  const auto dot = config.game.scripts.find_last_of('.');
  const std::string ext = dot == std::string::npos
                              ? config.game.scripts
                              : config.game.scripts.substr(dot);

  if (ext == ".rxdata")
    config.game.rgss = 1;
  else if (ext == ".rvdata")
    config.game.rgss = 2;
  else if (ext == ".rvdata2")
    config.game.rgss = 3;
}

const std::vector<ConfigField>& Fields() {
  static const std::vector<ConfigField> fields = {
      // Scripts comes first: the RGSS default is read off its extension, and
      // the Display defaults are read off the RGSS version.
      {"Game", "Scripts", ConfigPersist::kOnChange,
       [](const Config& config) { return config.game.scripts; },
       [](Config& config, const std::string& value) {
         config.game.scripts = value;
         ReplaceStringWidth(config.game.scripts, '\\', '/');
       },
       nullptr, nullptr},
      {"Game", "RGSS", ConfigPersist::kOnChange,
       [](const Config& config) { return std::to_string(config.game.rgss); },
       [](Config& config, const std::string& value) {
         config.game.rgss = ParseInteger(value, config.game.rgss);
       },
       &DeriveRGSSVersion, nullptr},
      // Title remains authored text: runtime changes are never persisted.
      {"Game", "Title", ConfigPersist::kNever,
       [](const Config& config) { return config.game.title; },
       [](Config& config, const std::string& value) {
         config.game.title = value;
       },
       nullptr, nullptr},
      {"Game", "RTP", ConfigPersist::kOnChange,
       [](const Config& config) { return config.game.rtp; },
       [](Config& config, const std::string& value) {
         config.game.rtp = value;
       },
       nullptr, nullptr},
      {"Game", "RTP1", ConfigPersist::kOnChange,
       [](const Config& config) { return config.game.rtp1; },
       [](Config& config, const std::string& value) {
         config.game.rtp1 = value;
       },
       nullptr, nullptr},
      {"Game", "RTP2", ConfigPersist::kOnChange,
       [](const Config& config) { return config.game.rtp2; },
       [](Config& config, const std::string& value) {
         config.game.rtp2 = value;
       },
       nullptr, nullptr},
      {"Game", "RTP3", ConfigPersist::kOnChange,
       [](const Config& config) { return config.game.rtp3; },
       [](Config& config, const std::string& value) {
         config.game.rtp3 = value;
       },
       nullptr, nullptr},

      {"Audio", "Soundfont", ConfigPersist::kOnChange,
       [](const Config& config) { return config.audio.soundfont; },
       [](Config& config, const std::string& value) {
         config.audio.soundfont = value;
       },
       nullptr, nullptr},
      {"Audio", "MasterVolume", ConfigPersist::kOnChange,
       [](const Config& config) {
         return std::to_string(config.audio.master_volume);
       },
       [](Config& config, const std::string& value) {
         config.audio.master_volume =
             ParseInteger(value, config.audio.master_volume);
       },
       nullptr, nullptr},
      {"Audio", "BGMVolume", ConfigPersist::kOnChange,
       [](const Config& config) {
         return std::to_string(config.audio.bgm_volume);
       },
       [](Config& config, const std::string& value) {
         config.audio.bgm_volume = ParseInteger(value, config.audio.bgm_volume);
       },
       nullptr, nullptr},
      {"Audio", "BGSVolume", ConfigPersist::kOnChange,
       [](const Config& config) {
         return std::to_string(config.audio.bgs_volume);
       },
       [](Config& config, const std::string& value) {
         config.audio.bgs_volume = ParseInteger(value, config.audio.bgs_volume);
       },
       nullptr, nullptr},
      {"Audio", "MEVolume", ConfigPersist::kOnChange,
       [](const Config& config) {
         return std::to_string(config.audio.me_volume);
       },
       [](Config& config, const std::string& value) {
         config.audio.me_volume = ParseInteger(value, config.audio.me_volume);
       },
       nullptr, nullptr},
      {"Audio", "SEVolume", ConfigPersist::kOnChange,
       [](const Config& config) {
         return std::to_string(config.audio.se_volume);
       },
       [](Config& config, const std::string& value) {
         config.audio.se_volume = ParseInteger(value, config.audio.se_volume);
       },
       nullptr, nullptr},
      {"Audio", "OutputDevice", ConfigPersist::kOnChange,
       [](const Config& config) { return config.audio.output_device; },
       [](Config& config, const std::string& value) {
         config.audio.output_device = value;
       },
       nullptr, nullptr},

      // A missing Width/Height starts from the size the RGSS version ships
      // with; the snapshot then keeps that default out of the file.
      {"Display", "Width", ConfigPersist::kOnChange,
       [](const Config& config) {
         return std::to_string(config.display.width);
       },
       [](Config& config, const std::string& value) {
         config.display.width = ParseInteger(value, config.display.width);
       },
       nullptr,
       [](const Config& config) {
         return std::to_string(config.xp() ? 640 : 544);
       }},
      {"Display", "Height", ConfigPersist::kOnChange,
       [](const Config& config) {
         return std::to_string(config.display.height);
       },
       [](Config& config, const std::string& value) {
         config.display.height = ParseInteger(value, config.display.height);
       },
       nullptr,
       [](const Config& config) {
         return std::to_string(config.xp() ? 480 : 416);
       }},
      {"Display", "VSync", ConfigPersist::kOnChange,
       [](const Config& config) { return BooleanText(config.display.vsync); },
       [](Config& config, const std::string& value) {
         config.display.vsync = ParseBoolean(value, config.display.vsync);
       },
       nullptr, nullptr},
      {"Display", "Fullscreen", ConfigPersist::kOnChange,
       [](const Config& config) {
         return BooleanText(config.display.fullscreen);
       },
       [](Config& config, const std::string& value) {
         config.display.fullscreen =
             ParseBoolean(value, config.display.fullscreen);
       },
       nullptr, nullptr},
      {"Display", "FrameSkip", ConfigPersist::kOnChange,
       [](const Config& config) {
         return BooleanText(config.display.frame_skip);
       },
       [](Config& config, const std::string& value) {
         config.display.frame_skip =
             ParseBoolean(value, config.display.frame_skip);
       },
       nullptr, nullptr},
      {"Display", "BackgroundRunning", ConfigPersist::kOnChange,
       [](const Config& config) {
         return BooleanText(config.display.background_running);
       },
       [](Config& config, const std::string& value) {
         config.display.background_running =
             ParseBoolean(value, config.display.background_running);
       },
       nullptr, nullptr},

      {"Window", "Resizable", ConfigPersist::kOnChange,
       [](const Config& config) {
         return ResizableName(config.display.resizable);
       },
       [](Config& config, const std::string& value) {
         config.display.resizable = ParseResizable(value);
       },
       nullptr, nullptr},

      {"GFX", "Backend", ConfigPersist::kOnChange,
       [](const Config& config) { return config.gfx.backend; },
       [](Config& config, const std::string& value) {
         config.gfx.backend = value;
       },
       nullptr, nullptr},
      {"GFX", "SpriteBatch", ConfigPersist::kOnChange,
       [](const Config& config) {
         return BooleanText(config.gfx.sprite_batch);
       },
       [](Config& config, const std::string& value) {
         config.gfx.sprite_batch = ParseBoolean(value, config.gfx.sprite_batch);
       },
       nullptr, nullptr},
      {"GFX", "PresentSampler", ConfigPersist::kOnChange,
       [](const Config& config) { return config.gfx.present_sampler; },
       [](Config& config, const std::string& value) {
         config.gfx.present_sampler = value;
       },
       nullptr, nullptr},
      {"GFX", "Upscaler", ConfigPersist::kOnChange,
       [](const Config& config) { return config.gfx.upscaler; },
       [](Config& config, const std::string& value) {
         config.gfx.upscaler = value;
       },
       nullptr, nullptr},

      {"Input", "PadBindings", ConfigPersist::kOnChange,
       [](const Config& config) {
         return BooleanText(config.input.pad_bindings);
       },
       [](Config& config, const std::string& value) {
         config.input.pad_bindings =
             ParseBoolean(value, config.input.pad_bindings);
       },
       nullptr, nullptr},
  };
  return fields;
}

const ConfigField* FindField(const std::string& section,
                             const std::string& key) {
  for (const auto& field : Fields())
    if (section == field.section && key == field.key)
      return &field;
  return nullptr;
}

// Patch legacy byte-oriented INIs one value at a time. Unchanged titles,
// comments and paths retain their exact original bytes. This also lets a web
// build (whose browser only encodes UTF-8) save ordinary ASCII settings.
bool PatchLegacy(const ini::IniFile& current,
                 const std::string& original_text,
                 const std::string& original_bytes,
                 const std::string& encoding,
                 std::string& output) {
  ini::IniFile before(original_text, true);
  ini::IniFile bytes(original_bytes, true);
  const auto encode = [&](const std::string& value, std::string& result) {
    return ini_encoding::Convert(value, "UTF-8", encoding, result);
  };
  for (const auto& section : current.GetSectionNames()) {
    for (const auto& key : current.GetKeys(section)) {
      const auto value = current.Get(section, key);
      if (before.HasKey(section, key) && before.Get(section, key) == value)
        continue;
      std::string encoded_section, encoded_key, encoded_value;
      if (!encode(section, encoded_section) || !encode(key, encoded_key) ||
          !encode(value, encoded_value))
        return false;
      bytes.Set(encoded_section, encoded_key, encoded_value);
    }
  }
  for (const auto& section : before.GetSectionNames()) {
    for (const auto& key : before.GetKeys(section)) {
      if (current.HasKey(section, key))
        continue;
      std::string encoded_section, encoded_key;
      if (!encode(section, encoded_section) || !encode(key, encoded_key))
        return false;
      bytes.RemoveKey(encoded_section, encoded_key);
    }
  }
  output = bytes.RenderPatched();
  return true;
}

}

Config::Config(URGE_EXCEPT, std::string inifile)
    : inifile_(std::move(inifile)) {
  auto stream = IOService::Get().OpenReadRaw(exception_state, inifile_);
  if (stream) {
    size_t data_size = 0;
    auto* data_ptr = SDL_LoadFile_IO(stream, &data_size, true);
    if (!data_ptr) {
      exception_state.Throw(ExceptionState::kIOError, "Cannot read {}: {}",
                            inifile_, SDL_GetError());
      return;
    }
    std::string bytes(static_cast<char*>(data_ptr), data_size);
    SDL_free(data_ptr);
    std::string data, error;
    save_enabled_ = ini_encoding::Decode(bytes, data, encoding_, bom_, error);
    if (!save_enabled_)
      LOGGER_WARN("[Config] {}: {}", inifile_, error);
    else if (encoding_ != "UTF-8")
      LOGGER_INFO("[Config] Reading {} as {}", inifile_, encoding_);
    const bool unicode_encoding =
        SDL_strncasecmp(encoding_.c_str(), "UTF", 3) == 0 ||
        SDL_strncasecmp(encoding_.c_str(), "UCS", 3) == 0;
    if (save_enabled_ && bom_.empty() && !unicode_encoding) {
      original_bytes_ = std::move(bytes);
      original_text_ = data;
    }
    parser_.LoadFromString(bom_ == "\xEF\xBB\xBF" ? bom_ + data : data);
  }

  if (exception_state.HadException())
    return;
  Load();
}

std::string Config::GetValue(std::string section,
                             std::string key,
                             std::string defval) {
  if (const auto* field = FindField(section, key))
    return field->get(*this);
  return parser_.Get(section, key, defval);
}

void Config::SetValue(std::string section, std::string key, std::string value) {
  if (section == "Game" && key == "IniEncoding") {
    LOGGER_WARN("[Config] IniEncoding can only be changed before startup");
    return;
  }
  if (const auto* field = FindField(section, key)) {
    field->set(*this, value);
    return;
  }
  parser_.Set(section, key, value);
}

void Config::SetCommandLine(int argc, char* argv[]) {
  command_line.clear();
  if (argc > 1)
    command_line.reserve(static_cast<size_t>(argc) - 1);
  for (int i = 1; i < argc; ++i)
    if (argv[i])
      command_line.emplace_back(argv[i]);
}

bool Config::HasCommandLine(const std::string& token) const {
  return std::find(command_line.begin(), command_line.end(), token) !=
         command_line.end();
}

void Config::Save(URGE_EXCEPT) {
  if (!save_enabled_)
    return;
  const auto& fields = Fields();
  for (size_t i = 0; i < fields.size(); ++i) {
    const auto& field = fields[i];
    if (field.persist != ConfigPersist::kOnChange)
      continue;
    const std::string value = field.get(*this);
    if (i < baseline_.size() && value == baseline_[i])
      continue;
    parser_.Set(field.section, field.key, value);
  }

  std::vector<std::string> names;
  std::unordered_map<std::string, std::string> joined;
  for (const auto& binding : key_bindings) {
    auto [it, inserted] = joined.try_emplace(binding.name);
    if (inserted)
      names.push_back(binding.name);
    else
      it->second += ',';
    it->second += KeyNameOf(binding);
  }
  for (const auto& name : parser_.GetKeys("KeyBinding"))
    if (joined.find(name) == joined.end())
      parser_.RemoveKey("KeyBinding", name);
  for (const auto& name : names)
    parser_.Set("KeyBinding", name, joined[name]);

  if (!parser_.HasChanges())
    return;

  // Finish the conversion before opening (and truncating) the original file.
  std::string text = parser_.RenderPatched();
  if (!ini_encoding::IsUTF8(text)) {
    exception_state.Throw(ExceptionState::kIOError,
                          "Cannot save {}: invalid UTF-8 configuration value",
                          inifile_);
    return;
  }
  if (encoding_ != "UTF-8") {
    std::string encoded;
    const bool converted =
        original_bytes_.empty()
            ? ini_encoding::Convert(text, "UTF-8", encoding_, encoded)
            : PatchLegacy(parser_, original_text_, original_bytes_, encoding_,
                          encoded);
    if (!converted) {
      exception_state.Throw(ExceptionState::kIOError,
                            "Cannot save {}: a value cannot be encoded as {}",
                            inifile_, encoding_);
      return;
    }
    text = bom_ + encoded;
  }
  auto* stream = IOService::Get().OpenWrite(exception_state, inifile_);
  if (!stream)
    return;
  SDL_WriteIO(stream, text.data(), text.size());
  SDL_CloseIO(stream);

  UpdateBaseline();
}

void Config::Load() {
  for (const auto& field : Fields()) {
    std::string value;
    if (parser_.HasKey(field.section, field.key))
      value = parser_.Get(field.section, field.key);
    else if (field.fallback)
      value = field.fallback(*this);
    else
      value = field.get(*this);

    field.set(*this, value);
    if (field.after_load)
      field.after_load(*this);
  }

  key_bindings.clear();
  for (const auto& symbol : parser_.GetKeys("KeyBinding"))
    for (const auto& name : SplitKeys(parser_.Get("KeyBinding", symbol)))
      key_bindings.push_back(ParseKeyBinding(symbol, name));

  UpdateBaseline();
}

void Config::UpdateBaseline() {
  const auto& fields = Fields();
  baseline_.resize(fields.size());
  for (size_t i = 0; i < fields.size(); ++i)
    baseline_[i] = fields[i].get(*this);
}

}
