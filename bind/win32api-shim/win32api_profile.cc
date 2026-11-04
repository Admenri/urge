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

#include <cstdlib>
#include "SDL3/SDL.h"
#include "bind/win32api-shim/win32api_shim.h"
#include "core/common/exception.h"
#include "core/common/inirw.h"
#include "core/common/logger.h"
#include "core/system/config.h"
#include "core/system/filesystem.h"
#include "core/system/ini_encoding.h"
namespace binding::win32api {
namespace {
Result Fail(uint32_t code) {
  SetLastError(code);
  return {};
}
bool StringOrNull(const Argument& value) {
  return value.buffer || value.null();
}
std::string Path(std::string_view input) {
  std::string path(input);
  for (char& ch : path)
    if (ch == '\\')
      ch = '/';
  while (path.starts_with("./"))
    path.erase(0, 2);
  return path;
}
bool CurrentIni(const std::string& path) {
  auto* config = urge::Config::TryGet();
  return config &&
         SDL_strcasecmp(path.c_str(), config->filename().c_str()) == 0;
}
std::string Section(const ini::IniFile& file, const std::string& name) {
  for (const auto& section : file.GetSectionNames())
    if (SDL_strcasecmp(section.c_str(), name.c_str()) == 0)
      return section;
  return name;
}
std::string Key(const ini::IniFile& file,
                const std::string& section,
                const std::string& name) {
  for (const auto& key : file.GetKeys(section))
    if (SDL_strcasecmp(key.c_str(), name.c_str()) == 0)
      return key;
  return name;
}
struct Profile {
  ini::IniFile file;
  std::string encoding = "UTF-8", bom;
  bool lossless = true;
};
bool ReadProfile(const std::string& path, Profile& profile) {
  auto* io = urge::IOService::TryGet();
  if (!io)
    return false;
  if (!io->Exists(path))
    return true;
  urge::ExceptionState error;
  SDL_IOStream* stream = io->OpenReadRaw(error, path);
  if (!stream)
    return false;
  size_t length = 0;
  void* bytes = SDL_LoadFile_IO(stream, &length, true);
  if (!bytes)
    return false;
  const std::string source(static_cast<char*>(bytes), length);
  SDL_free(bytes);
  std::string text, warning;
  profile.lossless = urge::ini_encoding::Decode(source, text, profile.encoding,
                                                profile.bom, warning);
  if (!profile.lossless)
    LOGGER_WARN("[Win32API] {}: {}", path, warning);
  profile.file.LoadFromString(text);
  return true;
}
Result GetProfileInt(std::span<const Argument> args) {
  if (!args[0].buffer || !args[1].buffer || !args[3].buffer)
    return Fail(87);
  const auto path = Path(args[3].text());
  const std::string section(args[0].text()), key(args[1].text());
  const std::string missing(1, '\x01');
  std::string value;
  if (CurrentIni(path)) {
    value = urge::Config::Get().ReadProfileValue(section, key, missing);
  } else {
    Profile profile;
    if (!ReadProfile(path, profile))
      return Result{args[2].number};
    const auto actual_section = Section(profile.file, section);
    value = profile.file.Get(actual_section,
                             Key(profile.file, actual_section, key), missing);
  }
  if (value == missing)
    return Result{args[2].number};
  return Result{
      static_cast<uint32_t>(std::strtoll(value.c_str(), nullptr, 10))};
}
Result WriteProfile(std::span<const Argument> args) {
  if (!args[0].buffer || !StringOrNull(args[1]) || !StringOrNull(args[2]) ||
      !args[3].buffer)
    return Fail(87);
  const auto path = Path(args[3].text());
  const std::string section(args[0].text()), key(args[1].text()),
      value(args[2].text());
  if (!urge::ini_encoding::IsUTF8(section) ||
      !urge::ini_encoding::IsUTF8(key) || !urge::ini_encoding::IsUTF8(value))
    return Fail(1113);
  urge::ExceptionState error;
  if (CurrentIni(path)) {
    urge::Config::Get().WriteProfileValue(error, section,
                                          args[1].null() ? nullptr : &key,
                                          args[2].null() ? nullptr : &value);
    if (error.HadException()) {
      LOGGER_WARN("[Win32API] {}", error.message());
      return Fail(1113);
    }
    return Result{1};
  }
  Profile profile;
  if (!ReadProfile(path, profile) || !profile.lossless)
    return Fail(1113);
  const auto actual_section = Section(profile.file, section);
  const auto actual_key = Key(profile.file, actual_section, key);
  if (args[1].null())
    profile.file.RemoveSection(actual_section);
  else if (args[2].null())
    profile.file.RemoveKey(actual_section, actual_key);
  else
    profile.file.Set(actual_section, actual_key, value);
  std::string text = profile.file.RenderPatched();
  if (profile.encoding != "UTF-8") {
    std::string encoded;
    if (!urge::ini_encoding::Convert(text, "UTF-8", profile.encoding, encoded))
      return Fail(1113);
    text = std::move(encoded);
  }
  text = profile.bom + text;
  auto* stream = urge::IOService::Get().OpenWrite(error, path);
  if (!stream)
    return Fail(5);
  const bool written =
      SDL_WriteIO(stream, text.data(), text.size()) == text.size();
  const bool closed = SDL_CloseIO(stream);
  return written && closed ? Result{1} : Fail(5);
}
}  // namespace
void RegisterProfiles(Registry& registry) {
  registry.Add("kernel32", "GetPrivateProfileInt", 4, 0, GetProfileInt);
  registry.Add("kernel32", "GetPrivateProfileIntA", 4, 0, GetProfileInt);
  registry.Add("kernel32", "WritePrivateProfileString", 4, 0, WriteProfile);
  registry.Add("kernel32", "WritePrivateProfileStringA", 4, 0, WriteProfile);
}
}  // namespace binding::win32api
