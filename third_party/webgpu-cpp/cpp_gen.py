import os
import re
import sys
import json
import jinja2

class WGPUGenerator:
  def __init__(self):
    cwd = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(cwd, "template.hpp")
    with open(path, "r") as file:
      template_text = file.read()
    self._environment = jinja2.Environment(
        extensions=['jinja2.ext.do', 'jinja2.ext.loopcontrols'],
        lstrip_blocks=True,
        trim_blocks=True,
        line_comment_prefix='//*')
    self._template = self._environment.from_string(template_text)

  def is_type_a(self, type, target):
    if type.startswith("WGPU"):
      type = type[4:]
    return type in self._data[target].keys()

  def as_cpptype(self, ctype, in_struct=False):
    ctype = ctype.replace("WGPU_NULLABLE", "").strip()
    if ctype == "WGPUProc":
      return ctype
    if ctype == "WGPUStatus":
      return "ConvertibleStatus"
    if ctype.startswith("WGPU"):
      if ctype.endswith("CallbackInfo"):
        return ctype
      if self.is_type_a(ctype, "class") and not in_struct:
        return ctype[4:] + " const&"
      return ctype[4:]
    return ctype

  def as_cppvalue(self, cvalue):
    if cvalue == "NULL":
      return "nullptr"
    if cvalue == "WGPU_TRUE":
      return "true"
    if cvalue == "WGPU_FALSE":
      return "false"
    if cvalue.startswith("_wgpu_ENUM_ZERO_INIT"):
      return "{}"
    if cvalue.startswith("WGPU"):
      # STRUCT INIT
      if cvalue.endswith("_INIT"):
        return "{}"
      # CONST DEFINE
      if cvalue.startswith("WGPU_"):
        return cvalue
      
      if self.is_type_a(cvalue, "class"):
        # CLASS
        return cvalue[4:]
      else:
        # ENUM
        enum_parts = cvalue.split("_", 1)
        enum_class = enum_parts[0][4:]
        enum_value = enum_parts[1]
        if enum_value[0].isdigit():
          enum_value = "e" + enum_value
        return enum_parts[0][4:] + "::" + enum_value
    return cvalue

  def render(self, json_data):
    self._data = json_data
    render_params = {
      "as_cppvalue": lambda value: self.as_cppvalue(value),
      "as_cpptype": lambda ctype, in_struct=False: self.as_cpptype(ctype, in_struct),
      "json_data": json_data,
      "print": print,
      "list": list,
      "len": len,
    }
    return self._template.render(render_params)

if __name__ == "__main__":
  cwd = os.path.dirname(os.path.abspath(__file__))
  path = os.path.join(cwd, "webgpu.h.json")
  with open(path, "r") as file:
    data = json.loads(file.read())

  gen = WGPUGenerator()
  header = gen.render(data)

  path = os.path.join(cwd, "webgpu_cpp.hpp")
  with open(path, "w+") as file:
    file.write(header)
