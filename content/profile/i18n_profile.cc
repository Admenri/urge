// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/profile/i18n_profile.h"

#include "rapidxml/rapidxml.hpp"

#include "base/memory/allocator.h"

#if defined(RAPIDXML_NO_EXCEPTIONS)

void rapidxml::parse_error_handler(const char* what, void* where) {
  LOG(ERROR) << what << where;
  std::abort();
}

#endif  // RAPIDXML_NO_EXCEPTIONS

namespace content {

I18NProfile::I18NProfile(SDL_IOStream* xml_stream) {
  if (!xml_stream) {
    LOG(INFO) << "[i18n] Failed to load XML i18n resource.";
    return;
  }

  int64_t stream_size = SDL_GetIOSize(xml_stream);
  if (stream_size) {
    char* stream_buf = static_cast<char*>(SDL_malloc(stream_size + 1));
    if (stream_buf) {
      stream_buf[stream_size] = '\0';
      SDL_ReadIO(xml_stream, stream_buf, stream_size);

      base::OwnedPtr<rapidxml::xml_document<>> xml_document =
          base::MakeOwnedPtr<rapidxml::xml_document<>>();
      xml_document->parse<rapidxml::parse_default>(stream_buf);

      rapidxml::xml_node<>* root =
          xml_document->first_node("translationbundle");
      if (root) {
        if (rapidxml::xml_attribute<>* lang = root->first_attribute("lang"))
          LOG(INFO) << "[Locate] Engine language: " << lang->value();

        for (rapidxml::xml_node<>* iter = root->first_node("translation"); iter;
             iter = iter->next_sibling("translation")) {
          rapidxml::xml_attribute<>* attr = iter->first_attribute("id");
          if (attr) {
            // Insert i18n translation
            i18n_translation_.emplace(std::stoi(attr->value()),
                                      base::String(iter->value()));
          }
        }
      }

      SDL_free(stream_buf);
    }
  }

  SDL_CloseIO(xml_stream);
}

I18NProfile::~I18NProfile() = default;

base::String I18NProfile::GetI18NString(int32_t ids,
                                       const base::String& default_value) {
  auto iter = i18n_translation_.find(ids);
  if (iter != i18n_translation_.end())
    return iter->second;
  return default_value;
}

}  // namespace content
