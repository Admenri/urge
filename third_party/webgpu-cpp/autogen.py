import argparse
import json
import parser
import cpp_gen
import os

def should_write_file(filepath, content):
  if not os.path.exists(filepath):
    return True
  
  try:
    with open(filepath, 'r') as file:
      existing_content = file.read()
    return existing_content != content
  except Exception:
    return True

def write_file_if_changed(filepath, content):
  if should_write_file(filepath, content):
    with open(filepath, 'w+') as file:
      file.write(content)
    print(f"Written: {filepath}")
    return True
  else:
    print(f"Skipped (unchanged): {filepath}")
    return False

if __name__ == "__main__":
  argv_parser = argparse.ArgumentParser(description="WebGPU Header Generator")
  argv_parser.add_argument("--header", help="Header for parsing", type=str, required=True)
  argv_parser.add_argument("--output", help="Header output path", type=str, required=True)
  args = argv_parser.parse_args()

  # Write C IDL
  with open(args.header, "r") as file:
      content = file.read()

  # Parse IDL
  parser_obj = parser.WGPUParser()
  parser_obj.parse(content)
  
  # Write JSON IDL
  json_content = json.dumps(parser_obj._parse_data, indent=2)
  write_file_if_changed(args.output + ".json", json_content)

  # Generate CPP Header
  gen = cpp_gen.WGPUGenerator()
  cpp_header = gen.render(parser_obj._parse_data)

  # Write CPP Header
  write_file_if_changed(args.output, cpp_header)