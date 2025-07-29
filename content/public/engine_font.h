// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_FONT_H_
#define CONTENT_PUBLIC_ENGINE_FONT_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_color.h"

namespace content {

/*--urge(name:Font)--*/
class URGE_OBJECT(Font) {
 public:
  virtual ~Font() = default;

  /*--urge(name:initialize,optional:size=20)--*/
  static scoped_refptr<Font> New(ExecutionContext* execution_context,
                                 const std::string& name,
                                 uint32_t size,
                                 ExceptionState& exception_state);

  /*--urge(name:initialize_copy)--*/
  static scoped_refptr<Font> Copy(ExecutionContext* execution_context,
                                  scoped_refptr<Font> other,
                                  ExceptionState& exception_state);

  /*--urge(name:exist?)--*/
  static bool IsExisted(ExecutionContext* execution_context,
                        const std::string& name,
                        ExceptionState& exception_state);

  /*--urge(name:default_name)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultName, std::vector<std::string>);

  /*--urge(name:default_size)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultSize, uint32_t);

  /*--urge(name:default_bold)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultBold, bool);

  /*--urge(name:default_italic)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultItalic, bool);

  /*--urge(name:default_shadow)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultShadow, bool);

  /*--urge(name:default_outline)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultOutline, bool);

  /*--urge(name:default_solid)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultSolid, bool);

  /*--urge(name:default_color)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultColor, scoped_refptr<Color>);

  /*--urge(name:default_out_color)--*/
  URGE_EXPORT_STATIC_ATTRIBUTE(DefaultOutColor, scoped_refptr<Color>);

  /*--urge(name:name)--*/
  URGE_EXPORT_ATTRIBUTE(Name, std::vector<std::string>);

  /*--urge(name:size)--*/
  URGE_EXPORT_ATTRIBUTE(Size, uint32_t);

  /*--urge(name:bold)--*/
  URGE_EXPORT_ATTRIBUTE(Bold, bool);

  /*--urge(name:italic)--*/
  URGE_EXPORT_ATTRIBUTE(Italic, bool);

  /*--urge(name:outline)--*/
  URGE_EXPORT_ATTRIBUTE(Outline, bool);

  /*--urge(name:shadow)--*/
  URGE_EXPORT_ATTRIBUTE(Shadow, bool);

  /*--urge(name:solid)--*/
  URGE_EXPORT_ATTRIBUTE(Solid, bool);

  /*--urge(name:color)--*/
  URGE_EXPORT_ATTRIBUTE(Color, scoped_refptr<Color>);

  /*--urge(name:out_color)--*/
  URGE_EXPORT_ATTRIBUTE(OutColor, scoped_refptr<Color>);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_FONT_H_
