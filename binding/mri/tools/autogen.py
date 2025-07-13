# Copyright 2018-2025 Admenri.
# Use of this source code is governed by a BSD - style license that can be
# found in the LICENSE file.

import sys
import os
import json
import bindgen as bgen

if __name__ == "__main__":
  in_dir = sys.argv[1]
  out_dir = sys.argv[2]
  
  current_directory = os.getcwd()
  idl_dir = os.path.join(current_directory, "doc")
  idl_dir = os.path.join(idl_dir, "api")

  os.makedirs(idl_dir, exist_ok=True)
  os.makedirs(out_dir, exist_ok=True)
  try:
    os.remove(os.path.join(idl_dir, "export_apis.json"))
  except FileNotFoundError:
    pass

  template_classes = []
  for filepath in os.listdir(in_dir):
    with open(os.path.join(in_dir, filepath), 'r', encoding='utf-8') as file:
      file_content = file.read()

      idl_parser = bgen.api_parser.APIParser()
      idl_parser.parse(file_content)
      for klass in idl_parser.classes:
        klass['filename'] = filepath
        template_classes.append(klass)

  with open(os.path.join(idl_dir, "export_apis.json"), "w", encoding="utf-8") as f:
    f.write(json.dumps(template_classes))

  for filepath in os.listdir(out_dir):
    os.remove(os.path.join(out_dir, filepath))

  header_collection = []
  init_function_collection = []
  for klass in template_classes:
    generator = bgen.MriBindingGen()
    generator.setup(template_classes, klass)

    source, filename, init_function = generator.generate_header()
    header_collection.append(filename)
    init_function_collection.append(init_function)
    with open(os.path.join(out_dir, filename), "w", encoding="utf-8") as f:
      f.write(source)

    source, filename = generator.generate_body()
    with open(os.path.join(out_dir, filename), "w", encoding="utf-8") as f:
      f.write(source)

  # Generate global include header for initialize
  header_includes = ""
  function_callings = ""

  for header in header_collection:
    header_includes += f"#include \"binding/mri/{header}\"\n"

  for fn in init_function_collection:
    function_callings += f"  {fn};\n"

  header_content = f"""
#ifndef BINDING_MRI_MRI_INIT_AUTOGEN_H_
#define BINDING_MRI_MRI_INIT_AUTOGEN_H_

{header_includes}

namespace binding {{

inline void InitMriAutogen() {{
{function_callings}
}}

}} // namespace binding

#endif // !BINDING_MRI_MRI_INIT_AUTOGEN_H_
"""

  with open(os.path.join(out_dir, "mri_init_autogen.h"), "w", encoding="utf-8") as f:
    f.write(header_content)
