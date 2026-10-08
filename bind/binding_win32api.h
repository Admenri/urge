#pragma once

// Hand-written binding: `Win32API` is not an engine class and has no export
// block in `core/`, so `bind/generate_binding.py` neither generates this pair
// nor registers it.  It is named from `generate_binding.py`'s
// `EXTRA_INIT_BINDING_CALLS` instead, which is where the global `InitBindings()`
// picks up initialisers the IR does not describe.
//
// The implementation lives entirely in `binding_win32api.cc`; the only thing
// other translation units need is the entry point below.

#include "cruby_utils.h"

namespace binding {

// Defines the `Win32API` class. Requires a live Ruby VM, like every other
// `Init*Binding()`.
void InitWin32APIBinding();

}  // namespace binding
