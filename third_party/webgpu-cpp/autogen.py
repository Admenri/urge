import argparse
import json
import parser
import cpp_gen

if __name__ == "__main__":
  argv_parser = argparse.ArgumentParser(description="WebGPU Header Generator")
  argv_parser.add_argument("--header", help="Header for parsing", type=str, required=True)
  argv_parser.add_argument("--output", help="Header output path", type=str, required=True)
  args = argv_parser.parse_args()

  # Write C IDL
  with open(args.header, "r") as file:
    content = file.read()

  # Parse IDL
  parser = parser.WGPUParser()
  parser.parse(content)
  
  # Write JSON IDL
  with open(args.output + ".json", "w+") as file:
    file.write(json.dumps(parser._parse_data))

  # Generate CPP Header
  gen = cpp_gen.WGPUGenerator()
  cpp_header = gen.render(parser._parse_data)

  # Write CPP Header
  with open(args.output, "w+") as file:
    file.write(cpp_header)
