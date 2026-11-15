// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
#pragma once

#include "ruby.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private, fixed-vendored-CRuby adapter; never expose VM layouts to C++.
typedef VALUE (*urge_script_entry)(int, VALUE*, VALUE);
void urge_fiber_set_root_proc(VALUE proc);
int urge_profile_frame_matches_entry(VALUE frame, urge_script_entry entry);

#ifdef __cplusplus
}
#endif
