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

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/definition.h"
#include "core/inirw.h"
#include "core/object.h"

namespace urge {

URGE_BINDING()
class Config : public Singleton<Config> {
 public:
  Config(std::string inifile);

  URGE_BINDING()
  std::string GetValue(std::string section,
                       std::string key,
                       std::string defval = {});

  URGE_BINDING()
  void SetValue(std::string section, std::string key, std::string value);

  void SetCommandLine(int argc, char* argv[]);

  bool HasCommandLine(const std::string& token) const;

  struct {
    int32_t rgss = 0;
    std::string scripts = "Data/Scripts.rxdata";
    std::string title = "(*^▽^*)";
    std::string rtp;
    std::string rtp1;
    std::string rtp2;
    std::string rtp3;
  } game;

  struct {
    std::string soundfont = "Fonts/Default.sf2";
    int32_t master_volume = 100;
    int32_t bgm_volume = 100;
    int32_t bgs_volume = 100;
    int32_t me_volume = 100;
    int32_t se_volume = 100;
    std::string output_device;
  } audio;

  struct {
    int32_t width = 640;
    int32_t height = 480;
    bool vsync = false;
    bool fullscreen = false;
    bool frame_skip = false;
  } display;

  struct {
    std::string backend = {};
    bool sprite_batch = true;
  } gfx;

  std::vector<std::pair<std::string, int32_t>> key_bindings;

  std::vector<std::string> command_line;

 public:
  bool xp() const { return game.rgss == 1; }
  bool vx() const { return game.rgss == 2; }
  bool vxa() const { return game.rgss == 3; }

  void Save();

 private:
  void Load();

  std::string inifile_;
  ini::IniFile parser_;
};

}  // namespace urge
