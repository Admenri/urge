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

#include <utility>

#include "cruby_utils.h"

#include "binding_render_graph_pass3d.h"

#include "core/scene3d/render_graph3d.h"

namespace binding {

class RubyGraphPassProc3D : public urge::GraphPassProc3D {
 public:
  explicit RubyGraphPassProc3D(VALUE callback) : callback_(callback) {
    rb_gc_register_address(&callback_);
  }

  ~RubyGraphPassProc3D() override { rb_gc_unregister_address(&callback_); }

  void Invoke(urge::RenderGraphPass3D* pass) override {
    VALUE wrapped =
        WrapObject<urge::RenderGraphPass3D>(pass, kRenderGraphPass3DDataType);
    rb_funcall(callback_, rb_intern("call"), 1, wrapped);
  }

  VALUE callback() const { return callback_; }

 private:
  VALUE callback_ = Qnil;
};

RB_FUNC(RenderGraphPass3D_Proc) {
  auto* pass = GetSelfData<urge::RenderGraphPass3D>(self);
  auto proc = pass->GetProc();
  if (!proc)
    return Qnil;

  auto* ruby_proc = dynamic_cast<RubyGraphPassProc3D*>(proc.get());
  return ruby_proc ? ruby_proc->callback() : Qnil;
}

RB_FUNC(RenderGraphPass3D_ProcEqual) {
  auto* pass = GetSelfData<urge::RenderGraphPass3D>(self);

  VALUE callback = Qnil;
  ParseArgs(argc, argv, "o", &callback);

  EXC_BEGIN {
    if (NIL_P(callback)) {
      pass->SetProc(nullptr);
    } else {
      urge::RefPtr<urge::GraphPassProc3D> proc =
          urge::MakeRefCounted<RubyGraphPassProc3D>(callback);
      pass->SetProc(std::move(proc));
    }
  }
  EXC_END;

  return callback;
}

inline void InitGraphPassProc3DBinding(VALUE klass) {
  DefineMethod(klass, "proc", RenderGraphPass3D_Proc);
  DefineMethod(klass, "proc=", RenderGraphPass3D_ProcEqual);
}

}
