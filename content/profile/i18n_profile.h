// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PROFILE_I18N_PROFILE_H_
#define CONTENT_PROFILE_I18N_PROFILE_H_

#include <string>
#include <unordered_map>

#include "components/filesystem/io_service.h"

namespace content {

class I18NProfile {
 public:
  I18NProfile(SDL_IOStream* xml_stream);
  ~I18NProfile();

  I18NProfile(const I18NProfile&) = delete;
  I18NProfile& operator=(const I18NProfile&) = delete;

  base::String GetI18NString(int32_t ids, const base::String& default_value);

 private:
  base::String i18n_xml_path_;
  std::unordered_map<int32_t, base::String> i18n_translation_;
};

}  // namespace content

#endif  // !CONTENT_PROFILE_I18N_PROFILE_H_
