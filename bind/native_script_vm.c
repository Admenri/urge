// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
#include "bind/native_script_vm.h"

// This adapter is intentionally C: the pinned Ruby 4 VM headers are not C++
// headers. Use the actual vendored layouts/APIs, never duplicate their offsets.
#include "vm_core.h"

void
urge_fiber_set_root_proc(VALUE proc)
{
    rb_execution_context_t *ec = GET_EC();
    // cont.c chose the bridge Proc's LEP at first entry. Select the user's LEP
    // before executing that body, exactly as cont.c would for the original
    // first_proc. Its existing root_svar starts at Qfalse and remains owned by
    // this Fiber; do not copy/reset it on resume, return, or ensure unwinding.
    // The original Proc is retained by the bridge closure and guarded on its
    // machine stack, so this escaped EP lives for the entire execution. CRuby
    // keeps escaped EPs stable across compaction, just like first_proc's EP.
    ec->root_lep = rb_vm_proc_local_ep(proc);
}

int
urge_profile_frame_matches_entry(VALUE frame, urge_script_entry entry)
{
    const rb_callable_method_entry_t *me;
    if (!RB_TYPE_P(frame, T_IMEMO) || imemo_type(frame) != imemo_ment)
        return 0;
    me = (const rb_callable_method_entry_t *)frame;
    return me->def && me->def->type == VM_METHOD_TYPE_CFUNC &&
        me->def->body.cfunc.argc == -1 &&
        me->def->body.cfunc.func == (rb_cfunc_t)entry;
}
