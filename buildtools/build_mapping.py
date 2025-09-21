# Copyright 2018-2025 Admenri.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import json
import pathlib
import argparse


def get_relative_unix_paths(root_dir, verbose):
  rel_paths = {}
  root_path = pathlib.Path(root_dir).resolve()  # 获取绝对路径

  for item in root_path.rglob('*'):
    if item.is_file():
      filepath = item.relative_to(root_path)
      rel_path = str(filepath).replace('\\', '/')
      rel_key = rel_path.lower()
      if verbose:
        print(f"Path: {rel_key} -> {rel_path}")
      rel_paths[rel_key] = rel_path

  return rel_paths


def save_to_json(tree, output_file):
  with open(output_file, 'w', encoding='utf-8') as f:
    data = json.dumps(tree, ensure_ascii=False)
    data = "const asyncMappingData = " + data
    f.write(data)


def main():
  parser = argparse.ArgumentParser(description='Generate directory tree structure as JSON')
  parser.add_argument('-i', '--input', required=True, help='Root directory to scan')
  parser.add_argument('-o', '--output', required=True, help='JSON output file')
  parser.add_argument('-v', '--verbose', action='store_true', help='Output verbose information')
  args = parser.parse_args()

  if not os.path.isdir(args.input):
    print(f"Error: Directory not found: '{args.input}'")
    exit(1)

  try:
    if args.verbose:
      print(f"Building directory tree for: {args.input}")

    tree = get_relative_unix_paths(args.input, args.verbose)

    if args.verbose:
      print(f"Tree structure built. Saving to {args.output}")

    save_to_json(tree, args.output)

    if args.verbose:
      print("Operation completed successfully.")
    else:
      print(f"Successfully generated directory tree JSON at {args.output}")

  except Exception as e:
    print(f"Error occurred: {str(e)}")
    exit(1)


if __name__ == "__main__":
  main()
