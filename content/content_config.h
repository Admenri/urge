// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CONTENT_CONFIG_H_
#define CONTENT_CONTENT_CONFIG_H_

#include <stdint.h>
#include <optional>

#include "base/buildflags/build.h"
#include "base/buildflags/compiler_specific.h"

namespace content {
struct ExecutionContext;
}  // namespace content

#define URGE_OBJECT(ty) \
  ty:                   \
 public                 \
  base::RefCounted<ty>

#define URGE_EXPORT_ATTRIBUTE(name, type)       \
  virtual type Get_##name(ExceptionState&) = 0; \
  virtual void Put_##name(const type&, ExceptionState&) = 0
#define URGE_EXPORT_STATIC_ATTRIBUTE(name, type)              \
  static type Get_##name(ExecutionContext*, ExceptionState&); \
  static void Put_##name(ExecutionContext*, const type&, ExceptionState&)
#define URGE_EXPORT_SERIALIZABLE(type)                                         \
  static scoped_refptr<type> Deserialize(ExecutionContext*,                    \
                                         const std::string&, ExceptionState&); \
  static std::string Serialize(ExecutionContext*, scoped_refptr<type>,         \
                               ExceptionState&)
#define URGE_EXPORT_COMPARABLE(type) \
  virtual bool CompareWithOther(scoped_refptr<type>, ExceptionState&) = 0;

#define URGE_EXPORT_DISPOSABLE                               \
  virtual void Dispose(ExceptionState& exception_state) = 0; \
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;
#define URGE_DECLARE_DISPOSABLE                           \
  void Dispose(ExceptionState& exception_state) override; \
  bool IsDisposed(ExceptionState& exception_state) override;

#define URGE_DECLARE_OVERRIDE_ATTRIBUTE(name, type) \
  type Get_##name(ExceptionState&) override;        \
  void Put_##name(const type&, ExceptionState&) override

#define URGE_DEFINE_OVERRIDE_ATTRIBUTE(name, type, klass, getter, setter)    \
  type klass::Get_##name(ExceptionState& exception_state) getter;            \
  void klass::Put_##name(const type& value, ExceptionState& exception_state) \
      setter;

#define URGE_DECLARE_STATIC_ATTRIBUTE_READ(klass, name, type) \
  type klass::Get_##name(ExecutionContext* execution_context, \
                         ExceptionState& exception_state)
#define URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(klass, name, type) \
  void klass::Put_##name(ExecutionContext* execution_context,  \
                         const type& value, ExceptionState& exception_state)

#define URGE_ATTRIBUTE_VALUE_CHECK(v)                               \
  if (!v)                                                           \
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, \
                                      "attribute value is null");

#endif  //! CONTENT_CONTENT_CONFIG_H_
