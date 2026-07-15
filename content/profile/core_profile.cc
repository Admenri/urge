// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/profile/core_profile.h"

#include "yaml-cpp/emitter.h"
#include "yaml-cpp/node/convert.h"
#include "yaml-cpp/node/detail/impl.h"
#include "yaml-cpp/node/emit.h"
#include "yaml-cpp/node/impl.h"
#include "yaml-cpp/node/iterator.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/parse.h"

#include "components/filesystem/stream_wrapper.h"

namespace content {

CoreProfile::CoreProfile(SDL_IOStream* config_file) {
  // Prepare for streaming
  filesystem::SDLIOStreamBuf stream_buf(config_file, true);
  std::istream config_stream(&stream_buf);
  auto root_node = YAML::Load(config_stream);

  // Load config
  auto core_node = root_node["core"];
  {
    core.api_version = core_node["apiVersion"].as<uint32_t>(core.api_version);
    core.scripts = core_node["scripts"].as<std::string>(core.scripts);
  }

  auto window_node = root_node["window"];
  {
    window.title = window_node["title"].as<std::string>(window.title);
    if (window_node["size"].IsSequence()) {
      window.size.x = window_node["size"][0].as<int32_t>(window.size.x);
      window.size.y = window_node["size"][1].as<int32_t>(window.size.y);
    }
    window.resizable = window_node["resizable"].as<bool>(window.resizable);
  }

  auto font_node = root_node["font"];
  {
    font.default_path = font_node["default"].as<std::string>(font.default_path);
  }
}

}  // namespace content
