import os
import re
import sys
import json

class WGPUParser:
  def __init__(self):
    self._stage = None
    self._line_cache = ""
    self._parse_data = {
      "enum": {},
      "bitmask": {},
      "class": {},
      "function": {},
      "struct": {},
    }

  def process_enum(self, block):
    enum_name = re.search(r'typedef enum (\w+)', block).group(1)
    members = re.findall(r'(\w+)\s*=', block)
    members_data = []
    for member in members:
      names = member.split("_", 1)
      members_data.append(names[1])
    self._parse_data["enum"][enum_name[4:]] = members_data

  def process_bitmask(self, block):
    pattern = r'static\s+const\s+\w+\s+(\w+)\s*='
    match = re.search(pattern, block)
    if match:
      names = match.group(1).split("_", 1)
      if self._parse_data["bitmask"].get(names[0][4:]) == None:
        self._parse_data["bitmask"][names[0][4:]] = []
      self._parse_data["bitmask"][names[0][4:]].append(names[1])

  def process_struct(self, block):
    block = block.replace('\n', '')
    struct_name_match = re.search(r'typedef\s+struct\s+(\w+)\s*{', block)
    struct_name = struct_name_match.group(1)
    content = block[block.find('{') + 1:block.find('}')]
    raw_members = content.split(";")

    struct_members = []
    for member in raw_members:
      parts = member.strip().rsplit(" ", 1)
      if len(parts) < 2:
        continue
      struct_members.append({
        "type": parts[0],
        "name": parts[1],
      })
    self._parse_data["struct"][struct_name[4:]] = struct_members

  def parse_wgpu_macro(self, text):
    text = text.replace('_wgpu_COMMA', ',').replace('\\', '')
    pattern = r'\/\*\.(\w+)=\*\/\s*(?:_wgpu_MAKE_INIT_STRUCT\([^,]+,\s*\{(.+?)\}\)|([^,]+))'
    matches = re.findall(pattern, text, re.DOTALL)
    
    results = {}
    for key, nested_content, simple_value in matches:
      if nested_content:
        results[key] = self.parse_wgpu_macro(nested_content)
      else:
        results[key] = simple_value.strip()
    return results

  def process_struct_init(self, block):
    struct_data = self.parse_wgpu_macro(block)
    struct_name = block[block.find("(") + 1:block.find(",")]
    for name, value in struct_data.items():
      for target in self._parse_data["struct"][struct_name[4:]]:
        if target["name"] == name:
          target["default"] = value

  def process_function(self, block):
    first_token = block.find("(")
    last_token = block.find(")")
    start_token = block.rfind(" wgpu")
    func_name = block[start_token + 1:first_token]
    params_decl = block[first_token + 1:last_token]
    params = params_decl.split(",")
    return_type = block[:start_token].replace("WGPU_EXPORT", "").strip()
    param_list = []
    for param in params:
      parts = param.strip().rsplit(" ", 1)
      if len(parts) < 2:
        continue
      param_list.append({
        "type": parts[0],
        "name": parts[1],
      })
    if len(param_list) >= 1 and not func_name.endswith("FreeMembers"):
      first_param = param_list[0]
      class_name = first_param["type"][4:]
      if func_name[4:].startswith(class_name):
        if self._parse_data["class"].get(class_name) == None:
          self._parse_data["class"][class_name] = {}
        param_list.pop(0)
        self._parse_data["class"][class_name][func_name[4:][len(class_name):]] = {
          "params": param_list,
          "return": return_type,
        }
        return
    self._parse_data["function"][func_name[4:]] = {
      "params": param_list,
      "return": return_type,
    }

  def process_line(self, line):
    # Enum
    if line.startswith("typedef enum"):
      self._stage = "enum"
    if self._stage == "enum" and line.startswith("}"):
      self._stage = None
      self._line_cache += line + "\n"
      self.process_enum(self._line_cache)
      self._line_cache = ""
      return

    # Bitmask
    if line.startswith("static const"):
      self.process_bitmask(line)
      return

    # Struct
    if line.startswith("typedef struct"):
      self._stage = "struct"
    if self._stage == "struct" and line.startswith("}"):
      self._stage = None
      self._line_cache += line + "\n"
      self.process_struct(self._line_cache)
      self._line_cache = ""
      return

    # Function (Std-Decl)
    if line.startswith("WGPU_EXPORT"):
      self.process_function(line)
      return

    # Function (No-Decl)
    line_part = line.split(" ")
    if len(line_part) > 1 and line_part[1].startswith("wgpu"):
      self.process_function(line)
      return

    # Struct Init
    if line.startswith("#define") and line.find("MAKE_INIT_STRUCT") != -1:
      self._stage = "struct.init"
    if self._stage == "struct.init" and line == "})":
      self._stage = None
      self._line_cache += line + "\n"
      self.process_struct_init(self._line_cache)
      self._line_cache = ""
      return

    # Pass
    if self._stage != None:
      if not line.startswith("//") and not line.startswith("/*") and not line.startswith("*/") and not line.startswith("*"):
        self._line_cache += line + "\n"
      elif self._stage == "struct.init":
        self._line_cache += line + "\n"
    pass

  def parse(self, code):
    for line in code.split("\n"):
      self.process_line(line.strip())

if __name__ == "__main__":
  cwd = os.path.dirname(os.path.abspath(__file__))
  path = os.path.join(cwd, "webgpu.h")
  with open(path, "r") as file:
    content = file.read()

  parser = WGPUParser()
  parser.parse(content)
  
  with open(path + ".json", "w+") as file:
    file.write(json.dumps(parser._parse_data))
