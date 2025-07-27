# Copyright 2018-2025 Admenri.
# Use of this source code is governed by a BSD - style license that can be
# found in the LICENSE file.

import sys
import os
import json
import bindgen as bgen
import hashlib

def calculate_file_md5(file_path, buffer_size=8192):
  md5_hash = hashlib.md5()
  with open(file_path, 'rb') as f:
    while chunk := f.read(buffer_size):
      md5_hash.update(chunk)
  return md5_hash.hexdigest()

def main_process():
  in_dir = sys.argv[1]
  out_dir = sys.argv[2]
  
  current_directory = os.getcwd()
  idl_dir = os.path.join(current_directory, "doc")
  idl_dir = os.path.join(idl_dir, "api")

  os.makedirs(idl_dir, exist_ok=True)
  os.makedirs(out_dir, exist_ok=True)

  # Calculate idl MD5
  api_hash_set = []
  for filepath in os.listdir(in_dir):
    api_hash_set.append(calculate_file_md5(os.path.join(in_dir, filepath)))
  current_api_hash = hashlib.md5(':'.join(api_hash_set).encode('utf-8')).hexdigest()
  
  # Check apis MD5
  exist_api_hash = "<No Hash>"
  hash_file_path = os.path.join(idl_dir, "api_hash.txt")
  if os.path.exists(hash_file_path):
    with open(hash_file_path, "r", encoding="utf-8") as f:
      exist_api_hash = f.read().strip()

  # Check
  print(f"Current API Hash: {current_api_hash}")
  print(f"Local API Hash: {exist_api_hash}")
  if current_api_hash == exist_api_hash:
    return

  # Save current api hash
  with open(hash_file_path, "w", encoding="utf-8") as f:
    f.write(current_api_hash.strip())
  
  template_classes = []  
  for filepath in os.listdir(in_dir):
    with open(os.path.join(in_dir, filepath), 'r', encoding='utf-8') as file:
      file_content = file.read()

      idl_parser = bgen.api_parser.APIParser()
      idl_parser.parse(file_content)
      for klass in idl_parser.classes:
        klass['filename'] = filepath
        template_classes.append(klass)
        
  # Gen json data
  apis_serialized = json.dumps(template_classes, sort_keys=True)

  # Update bindgen json
  try:
    os.remove(os.path.join(idl_dir, "export_apis.json"))
  except FileNotFoundError:
    pass
  with open(os.path.join(idl_dir, "export_apis.json"), "w", encoding="utf-8") as f:
    f.write(apis_serialized)

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


if __name__ == "__main__":
  main_process()
