# Copyright 2018-2025 Admenri.
# Use of this source code is governed by a BSD - style license that can be
# found in the LICENSE file.

import re
import json

class APIParser:
  def __init__(self):
    # 当前内容所识别到的全部类数据
    self.classes = []
    # 当前正在解析的类
    self.current_class = None
    # 解析器当前模式
    self.parsing_mode = ""
    # 当前注释声明的内容
    self.current_comment = {}
    # 通用多行文本缓存
    self.buffer = ""
    # 通用列表缓存
    self.list_cache = []

  # 解析 "/*--urge(***)--*/" 声明的函数
  def parse_urge_comment(self, line):
    # 通过正则表达式解包出内容
    match = re.search(r'/\*--urge\((.*)\)--\*/', line)
    return self.parse_params_dict(match.group(1)) if match else {}

  # 解析声明注释内容
  # 期望输入：xxx:123,bbb:aaa,ccc:abc=123
  @staticmethod
  def parse_params_dict(param_str):
    params = {}
    if not param_str: return params
    for item in param_str.split(','):
      item = item.strip()
      if not item: continue
      if ':' in item:
        k, v = item.split(':', 1)
        k, v = k.strip(), v.strip()
        if k in params:
          if isinstance(params[k], list):
            params[k].append(v)
          else:
            params[k] = [params[k], v]
        else:
          params[k] = v
      else:
        params[item] = True
    return params

  # 处理类的声明
  # 构造一个新的类数据模版
  def process_class(self, line):
    # 期望的格式：
    #  class URGE_OBJECT(xxxxx) {
    match = re.search(r'class \w+\((.*?)\)', line)
    if not match: return None

    # 将之前的类加入类数据库
    if self.current_class is not None:
      self.process_dependency()
      self.classes.append(self.current_class)

    # 构造新的类数据
    self.current_class = {
      "type": "class",
      "native_name": match.group(1),
      "binding_name": self.current_comment.get('name', None),
      "is_module": self.current_comment.get('is_module', False),
      "enums": [],
      "structs": [],
      "methods": [],
      "attributes": [],
      "dependency": [],
      "is_comparable": False,
      "is_serializable": False,
    }

  # 解析 enum 内容的函数，
  # 期望拿到的数据：
  #  enum XXX {
  #    // xxx
  #    CONST = 0,
  #    CONST2,
  #  };
  def process_enum(self, lines):
    enum_name = ''
    enum_range = None
    enum_constants = []

    # 统合换行代码
    intermediate_enum_code = ""

    # 逐行解析枚举体
    for line in lines:
      # 解析枚举名称
      if line.startswith('enum'):
        match = re.search(r'enum\s+(?:class|struct)?\s*(\w+)\s*(?::\s*(\w+))?\s*\{', line)
        enum_name = match.group(1)
        enum_range = match.group(2)
        continue
      # 末尾
      if line.startswith('};'):
        break
      # 单行注释
      if line.startswith('//'):
        continue

      # 加入多行内容
      intermediate_enum_code += line

    # 解析实际常量
    for line in intermediate_enum_code.strip().split(','):
      # 解析内容
      match = re.search(r'^\s*([A-Z_][A-Z0-9_]*)', line)
      if match:
        enum_constants.append(match.group(1))

    # 构造枚举信息
    enum_info = {
      "native_name": enum_name,
      "range": enum_range,
      "binding_name": self.current_comment.get('name', None),
      "constants": enum_constants,
    }

    # 添加到当前解析类数据中
    self.current_class['enums'].append(enum_info)

  # 解析形参类型数据
  # 本函数会递归解析类型的嵌套容器：
  #  1. base::Vector<scoped_refptr<xxx>>
  #  3. base::Vector<xxx>
  @staticmethod
  def parse_variable(varname):
    # 考虑以下情况：
    #   1. const Type &name
    #   2. Type name
    split_pos = len(varname)
    for i in range(-1, -len(varname) -1, -1):
      if not (varname[i].isalpha() or varname[i].isalnum() or varname[i] == '_'):
        split_pos = i
        break
    typename = varname[:split_pos]
    declname = varname[split_pos + 1:]

    # 原始类型
    type_raw = typename

    # 参数类型提纯（const ***& => ***）
    if typename.startswith('const'):
      first_token = typename.find(' ')
      last_token = typename.rfind('&')
      if last_token == -1:
        last_token = len(typename)
      typename = typename[first_token: last_token].strip()

    # 递归分析
    # 分析参数容器类型（struct array refptr）
    containers = []
    root_type = None
    def _recursive_parse_param(recursive_type):
      nonlocal containers
      nonlocal root_type
      if recursive_type.find('<') != -1 and recursive_type[-1] == '>':
        r_first_token = recursive_type.find('<')
        r_last_token = recursive_type.rfind('>')
        containers.append(recursive_type[0: r_first_token])
        _recursive_parse_param(recursive_type[r_first_token + 1: r_last_token].strip())
        return
      root_type = recursive_type

    # 分析
    _recursive_parse_param(typename)
    # 生成
    type_detail = {
      "root_type": root_type,
      "containers": containers,
    }

    return type_raw, type_detail, declname

  # 解析结构体内容的函数，
  # 期望拿到的数据：
  #  struct XXX {
  #    // xxx
  #    int32_t param1;
  #    std::optional<Struct2> param2;
  #    base::String param3 = "default";
  #    scoped_refptr<Klass1> param4 = nullptr;
  #  };
  def process_struct(self, lines):
    struct_name = ""
    struct_members = []

    # 逐行解析
    line_cache = ""
    for line in lines:
      line = line.strip()

      # 解析名称
      if line.startswith('struct'):
        match = re.search(r'struct \w+\((.*?)\)', line)
        struct_name = match.group(1)
        continue
      # 末尾
      if line.startswith('};'):
        break
      # 单行注释
      if line.startswith('//'):
        continue
      # 构造函数
      if line.startswith(struct_name):
        break

      # 将分布为多行的声明合并为一行处理
      line_cache += line

    # 处理声明主体
    for line in line_cache.split(';'):
      line = line.strip()
      if line == '':
        continue

      # 结构：
      #  1. 类型
      #  2. 类型 = 默认值
      formulas = line.split('=')

      # 类型名称
      type_name = formulas[0].strip()
      # 默认值（可能没有）
      default_value = formulas[1].strip() if len(formulas) > 1 else None

      # 解析类型名
      type_raw, type_details, var_name = self.parse_variable(type_name)

      # 单个成员解析完成
      struct_members.append({
        "native_name": var_name,
        "type_raw": type_raw,
        "type_detail": type_details,
        "default_value": default_value,
      })

    # 分析依赖
    deps = []
    for member in struct_members:
      dep = member['type_raw']
      first_sep = dep.rfind('<')
      last_sep = dep.find('>')
      if dep.find('scoped_refptr') != -1:
        if first_sep >= 0 or last_sep >= 0:
          dep = dep[first_sep + 1: last_sep]
          deps.append(dep)

    # 构造结构体信息
    struct_info = {
      "type": "struct",
      "native_name": struct_name,
      "binding_name": self.current_comment.get('name', None),
      "members": struct_members,
      "dependency": deps,
    }

    # 添加到当前解析类数据中
    self.classes.append(struct_info)

  # 解析属性，
  # 期望拿到的数据：
  #  URGE_EXPORT_ATTRIBUTE(yy, type);
  #  URGE_EXPORT_STATIC_ATTRIBUTE(xx, type);
  def process_attribute(self, lines):
    # 已知情况：
    #   URGE_EXPORT_ATTRIBUTE(name, type);
    #   URGE_EXPORT_STATIC_ATTRIBUTE(name, type);
    lines = ''.join(lines)

    # 直接取括号里的内容
    attr_infos = lines[lines.find('(') + 1: lines.rfind(')')]
    attr_infos = list(map(lambda x: x.strip(), attr_infos.split()))
    attr_infos = ''.join(attr_infos).split(',')

    # 属性提取
    attr_name = attr_infos[0].strip()
    attr_raw, attr_type, dummy = self.parse_variable(attr_infos[1] + " attr")

    # 解析类型
    attribute_info = {
      "binding_name": self.current_comment.get('name', attr_name),
      "native_name": attr_name,
      "type_raw": attr_raw,
      "type_detail": attr_type,
      "is_static": lines.strip().startswith("URGE_EXPORT_STATIC_ATTRIBUTE"),
    }

    # 加入当前类数据
    self.current_class['attributes'].append(attribute_info)

  # 解析成员函数信息
  # 期望输入：
  #  virtual scoped_refptr<xx> yy(ExceptionState& exception_state) = 0;
  #  static scoped_refptr<zz> New(ExecutionContext* execution_context,const base::String& name,uint32_t size,ExceptionState& exception_state);
  def process_member(self, lines):
    function_body = ''.join(lines)

    # 获取原始参数列表
    parameter_first = function_body.find('(')
    parameter_last = function_body.find(')')
    raw_parameters = function_body[parameter_first + 1: parameter_last]

    # 获取声明部分
    method_declaration = function_body[0: function_body.find('(')]
    method_info = method_declaration.split(' ')

    # 设置属性
    is_static = (method_info[0] == 'static')
    return_type = method_info[1]
    func_name = method_info[2]

    # 分割参数列表
    params_list = []
    raw_params_list = raw_parameters.split(',')

    # 解析参数列表
    for param_iter in raw_params_list:
      # 去除左右空格
      param_iter = param_iter.strip()

      # 过滤部分参数
      if param_iter.startswith('ExecutionContext') or param_iter.startswith('ExceptionState'):
        continue

      # 分析类型，形参名称
      type_raw, type_detail, var_name = self.parse_variable(param_iter)

      # 检查声明注释
      optional_params = {}
      if 'optional' in self.current_comment:
        opts = self.current_comment['optional']
        opts = [opts] if not isinstance(opts, list) else opts
        for opt in opts:
          if '=' in opt:
            k, v = opt.split('=', 1)
            optional_params[k.strip()] = v.strip()

      # 解析完成
      params_list.append({
        "native_name": var_name,
        "type_detail": type_detail,
        "type_raw": type_raw,
        "is_optional": var_name in optional_params,
        "default_value": optional_params.get(var_name, None),
      })

    # 寻找重载函数
    method_name = self.current_comment.get("name", None)
    method_func = func_name

    # 是否存在重载函数
    existing_overload = next(
      (m for m in self.current_class['methods']
       if m['binding_name'] == method_name
       and m['native_name'] == method_func
       and m['is_static'] == is_static),
      None
    )
    if existing_overload:
      existing_overload['params'].append(params_list)
      return

    # 添加到类数据
    return_type_raw, return_type_detail, dummy = self.parse_variable(return_type + " retval")

    # 生成成员信息
    member_info = {
      "binding_name": method_name,
      "native_name": method_func,
      "params": [params_list],
      "is_static": is_static,
      "return_type_raw": return_type_raw,
      "return_type_detail": return_type_detail,
    }

    # 添加到当前类
    self.current_class['methods'].append(member_info)

  # 单行内容的解析
  # 对于每一行内容的解析都需要访问当前解析器的上下文
  def process_line(self, line):
    # 去除两边的无用空白
    line = line.strip()

    # 处理多行声明注释
    if self.parsing_mode == "urge.declaration":
      self.buffer += line
      if line.rfind('--*/') != -1:
        # 多行声明结束
        self.current_comment = self.parse_urge_comment(self.buffer)
        self.parsing_mode = "expect.declaration"
      return

    # 处理 class 定义
    if self.parsing_mode == "class.parsing":
      self.buffer += line
      if '{' in line:
        # 结束 enum 解析
        self.process_class(self.buffer)
        self.parsing_mode = ""
      return

    # 处理 enum 常量定义
    if self.parsing_mode == "enum.parsing":
      self.list_cache.append(line)
      if line.rfind("};") != -1:
        # 结束 enum 解析
        self.process_enum(self.list_cache)
        self.parsing_mode = ""
      return

    # 处理结构体定义
    if self.parsing_mode == "struct.parsing":
      self.list_cache.append(line)
      if line.rfind("};") != -1:
        # 结束结构体解析
        self.process_struct(self.list_cache)
        self.parsing_mode = ""
      return

    # 处理属性定义
    if self.parsing_mode == "attribute.parsing":
      self.buffer += line
      if ';' in line:
        # 结束属性解析
        self.process_attribute(self.buffer)
        self.parsing_mode = ""
      return

    # 处理成员函数定义
    if self.parsing_mode == "member.parsing":
      self.buffer += line
      if ';' in line:
        # 结束成员函数解析
        self.process_member(self.buffer)
        self.parsing_mode = ""
      return

    # 无多行处理，开始解析新声明
    self.process_declaration(line)

  # 处理定义声明
  # /*--urge()--*/ 为识别前置声明
  # 其他注释与未声明内容则会被忽略
  def process_declaration(self, line):
    # 检测声明注释内容
    if line.startswith('/*--urge'):
      if line.rfind("--*/") != -1:
        # 声明注释在同一行，直接进行解析
        self.current_comment = self.parse_urge_comment(line)
        self.parsing_mode = "expect.declaration"
      else:
        # 声明注释为多行，开启多行累计解析模式
        self.buffer = line
        self.parsing_mode = "urge.declaration"
      return

    # 检测普通声明
    if self.parsing_mode == "expect.declaration":
      # 检测类声明
      if line.startswith("class"):
        # 如果为多行
        if '{' not in line:
          self.buffer = line
          self.parsing_mode = "class.parsing"
        else:
          # 仅占一行的情况
          self.process_class(line)
          self.parsing_mode = ""
        return

      # 枚举常量声明
      if line.startswith("enum"):
        # 由于使用Chromium编码风格，默认enum声明至少占3行，直接开启多行解析模式
        self.list_cache = [line]
        self.parsing_mode = "enum.parsing"
        return

      # 检测结构体
      if line.startswith("struct"):
        # 由于编码风格直接多行
        self.list_cache = [line]
        self.parsing_mode = "struct.parsing"
        return

      # 检测属性 "URGE_EXPORT_ATTRIBUTE" "URGE_EXPORT_STATIC_ATTRIBUTE"
      if line.startswith("URGE_EXPORT_ATTRIBUTE") or line.startswith("URGE_EXPORT_STATIC_ATTRIBUTE"):
        if ';' not in line:
          self.buffer = line
          self.parsing_mode = "attribute.parsing"
        else:
          # 仅占一行的情况
          self.process_attribute(line)
          self.parsing_mode = ""
        return

      # 检测属性：URGE_EXPORT_SERIALIZABLE
      if line.startswith("URGE_EXPORT_SERIALIZABLE"):
        self.current_class["is_serializable"] = True
        self.parsing_mode = ""
        return

      # 检测属性：URGE_EXPORT_COMPARABLE
      if line.startswith("URGE_EXPORT_COMPARABLE"):
        self.current_class["is_comparable"] = True
        self.parsing_mode = ""
        return

      # 成员函数
      if line.startswith("virtual") or line.startswith("static"):
        if ';' not in line:
          self.buffer = line
          self.parsing_mode = "member.parsing"
        else:
          # 仅占一行的情况
          self.process_member(line)
          self.parsing_mode = ""
        return

  # 处理当前类中的依赖类数据
  def process_dependency(self):
    dependency = [self.current_class["native_name"]]

    # 查找顺序：
    #  属性 -> 方法
    dependency_raw = []

    # 属性遍历
    attributes_data = self.current_class["attributes"]
    for attribute in attributes_data:
      dependency_raw.append(attribute["type_raw"])

    # 方法遍历
    methods_data = self.current_class["methods"]
    for method in methods_data:
      dependency_raw.append(method["return_type_raw"])
      for overload in method["params"]:
        for param in overload:
          dependency_raw.append(param["type_raw"])

    # 分析依赖
    for dep in dependency_raw:
      first_sep = dep.rfind('<')
      last_sep = dep.find('>')
      if dep.find('scoped_refptr') != -1:
        if first_sep >= 0 or last_sep >= 0:
          dep = dep[first_sep + 1: last_sep]
          dependency.append(dep)

    # 去重
    dependency = list(set(dependency))

    # 添加到当前类数据
    self.current_class["dependency"] = dependency

  # 逐行解析接口定义
  # 将整个头文件内容分割为单行后对每行进行分析
  def parse(self, code):
    for line in code.split('\n'):
      self.process_line(line)

    if self.current_class:
      self.process_dependency()
      self.classes.append(self.current_class)

if __name__ == "__main__":
  cpp_code = """
// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_BITMAP_H_
#define CONTENT_PUBLIC_ENGINE_BITMAP_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"
#include "content/public/engine_color.h"
#include "content/public/engine_font.h"
#include "content/public/engine_iostream.h"
#include "content/public/engine_rect.h"
#include "content/public/engine_surface.h"

namespace content {

  /*--urge(name:Size)--*/
  struct URGE_OBJECT(Size) {
    uint32_t width;
    uint32_t height;
  };
  
  /*--urge(name:CreateInfo)--*/
  struct URGE_OBJECT(CreateInfo) {
    uint32_t test;
    uint32_t id = 0;
    base::String filename = "null";
    std::optional<Size> size;
  };

// IDL generator format:
// Inhert: refcounted only.
// Interface reference: RGSS Reference
/*--urge(name:Bitmap)--*/
class URGE_OBJECT(Bitmap) {
 public:
  virtual ~Bitmap() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Bitmap> New(ExecutionContext* execution_context,
                                   const base::String& filename,
                                   const std::vector<scoped_refptr<Test>> test,
                                   ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Bitmap> New(ExecutionContext* execution_context,
                                   uint32_t width,
                                   uint32_t height,
                                   ExceptionState& exception_state);

  /*--urge(name:initialize_copy)--*/
  static scoped_refptr<Bitmap> Copy(ExecutionContext* execution_context,
                                    scoped_refptr<Bitmap> other,
                                    ExceptionState& exception_state);

  /*--urge(name:from_surface)--*/
  static scoped_refptr<Bitmap> FromSurface(ExecutionContext* execution_context,
                                           scoped_refptr<Surface> surface,
                                           ExceptionState& exception_state);

  /*--urge(name:from_stream)--*/
  static scoped_refptr<Bitmap> FromStream(ExecutionContext* execution_context,
                                          scoped_refptr<IOStream> stream,
                                          const base::String& extname,
                                          ExceptionState& exception_state);

  /*--urge(serializable)--*/
  URGE_EXPORT_SERIALIZABLE(Bitmap);

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:width)--*/
  virtual uint32_t Width(ExceptionState& exception_state) = 0;

  /*--urge(name:height)--*/
  virtual uint32_t Height(ExceptionState& exception_state) = 0;

  /*--urge(name:rect)--*/
  virtual scoped_refptr<Rect> GetRect(ExceptionState& exception_state) = 0;

  /*--urge(name:blt,optional:opacity=255,optional:blend_type=0)--*/
  virtual void Blt(int32_t x,
                   int32_t y,
                   scoped_refptr<Bitmap> src_bitmap,
                   scoped_refptr<Rect> src_rect,
                   uint32_t opacity,
                   int32_t blend_type,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:stretch_blt,optional:opacity=255,optional:blend_type=0)--*/
  virtual void StretchBlt(scoped_refptr<Rect> dest_rect,
                          scoped_refptr<Bitmap> src_bitmap,
                          scoped_refptr<Rect> src_rect,
                          uint32_t opacity,
                          int32_t blend_type,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:fill_rect)--*/
  virtual void FillRect(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        scoped_refptr<Color> color,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:fill_rect)--*/
  virtual void FillRect(scoped_refptr<Rect> rect,
                        scoped_refptr<Color> color,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:gradient_fill_rect)--*/
  virtual void GradientFillRect(int32_t x,
                                int32_t y,
                                uint32_t width,
                                uint32_t height,
                                scoped_refptr<Color> color1,
                                scoped_refptr<Color> color2,
                                bool vertical,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:gradient_fill_rect)--*/
  virtual void GradientFillRect(int32_t x,
                                int32_t y,
                                uint32_t width,
                                uint32_t height,
                                scoped_refptr<Color> color1,
                                scoped_refptr<Color> color2,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:gradient_fill_rect)--*/
  virtual void GradientFillRect(scoped_refptr<Rect> rect,
                                scoped_refptr<Color> color1,
                                scoped_refptr<Color> color2,
                                bool vertical,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:gradient_fill_rect)--*/
  virtual void GradientFillRect(scoped_refptr<Rect> rect,
                                scoped_refptr<Color> color1,
                                scoped_refptr<Color> color2,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:clear)--*/
  virtual void Clear(ExceptionState& exception_state) = 0;

  /*--urge(name:clear_rect)--*/
  virtual void ClearRect(int32_t x,
                         int32_t y,
                         uint32_t width,
                         uint32_t height,
                         ExceptionState& exception_state) = 0;

  /*--urge(name:clear_rect)--*/
  virtual void ClearRect(scoped_refptr<Rect> rect,
                         ExceptionState& exception_state) = 0;

  /*--urge(name:get_pixel)--*/
  virtual scoped_refptr<Color> GetPixel(int32_t x,
                                        int32_t y,
                                        ExceptionState& exception_state) = 0;

  /*--urge(name:set_pixel)--*/
  virtual void SetPixel(int32_t x,
                        int32_t y,
                        scoped_refptr<Color> color,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:hue_change)--*/
  virtual void HueChange(int32_t hue, ExceptionState& exception_state) = 0;

  /*--urge(name:blur)--*/
  virtual void Blur(ExceptionState& exception_state) = 0;

  /*--urge(name:radial_blur)--*/
  virtual void RadialBlur(int32_t angle,
                          int32_t division,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        const base::String& str,
                        int32_t align,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        const base::String& str,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(scoped_refptr<Rect> rect,
                        const base::String& str,
                        int32_t align,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(scoped_refptr<Rect> rect,
                        const base::String& str,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:text_size)--*/
  virtual scoped_refptr<Rect> TextSize(const base::String& str,
                                       ExceptionState& exception_state) = 0;

  /*--urge(name:get_surface)--*/
  virtual scoped_refptr<Surface> GetSurface(
      ExceptionState& exception_state) = 0;

  /*--urge(name:font)--*/
  URGE_EXPORT_ATTRIBUTE(Font, scoped_refptr<Font>);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_BITMAP_H_
    """

  parser = APIParser()
  parser.parse(cpp_code)
  print(json.dumps(parser.classes))
