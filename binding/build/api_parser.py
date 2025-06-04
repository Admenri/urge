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
    #  class URGE_RUNTIME_API xxxxx : public base::RefCounted<xxxxx> {
    match = re.search(r'class\s+(?:[A-Za-z0-9_]+\s+)*([A-Za-z0-9_]+)\s*:\s*public\s+[A-Za-z0-9_:]+<\1>', line)
    if not match: return None

    # 将之前的类加入类数据库
    if self.current_class is not None:
      self.process_dependency()
      self.classes.append(self.current_class)

    # 构造新的类数据
    self.current_class = {
      "name": self.current_comment.get('name', None),
      "class": match.group(1),
      "is_module": self.current_comment.get('is_module', False),
      "enums": [],
      "structs": [],
      "attributes": [],
      "methods": [],
      "is_serializable": False,
      "is_comparable": False,
      "dependency": [],
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

      # 解析内容
      match = re.search(r'^\s*([A-Z_][A-Z0-9_]*)\b(?!\s*=)', line)
      if match:
        enum_constants.append(match.group(1))

    # 构造枚举信息
    enum_info = {
      "name": self.current_comment.get('name', None),
      "type": enum_name,
      "range": enum_range,
      "constants": enum_constants,
    }

    # 添加到当前解析类数据中
    self.current_class['enums'].append(enum_info)

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
    for line in lines:
      # 解析名称
      if line.startswith('struct'):
        match = re.search(r'struct\s+(\w+)\s*\{', line)
        struct_name = match.group(1)
        continue
      # 末尾
      if line.startswith('};'):
        break
      # 单行注释
      if line.startswith('//'):
        continue

      # 匹配每行信息
      pattern = r"""
          ^\s*
          ([a-zA-Z_][\w:<>]*)  # 类型（可能含模板、命名空间）
          \s+
          ([a-zA-Z_]\w*)       # 变量名
          \s*
          (?:=\s*([^;]+))?     # 默认值（可选）
          \s*;\s*$
      """

      # 解析内容
      match = re.match(pattern, line.strip(), re.VERBOSE)
      if match:
        struct_members.append({
          "name": match.group(2),
          "type": match.group(1),
          "default_value": match.group(3),
        })

    # 构造结构体信息
    struct_info = {
      "name": self.current_comment.get('name', None),
      "type": struct_name,
      "members": struct_members,
    }

    # 添加到当前解析类数据中
    self.current_class['structs'].append(struct_info)

  # 解析属性，
  # 期望拿到的数据：
  #  URGE_EXPORT_ATTRIBUTE(yy, type);
  #  URGE_EXPORT_STATIC_ATTRIBUTE(xx, type);
  def process_attribute(self, lines):
    # 解析声明
    attribute_info = {}
    for pattern, is_static in [
      (r'URGE_EXPORT_ATTRIBUTE\((\w+),\s*(.*?)\);?', False),
      (r'URGE_EXPORT_STATIC_ATTRIBUTE\((\w+),\s*(.*?)\);?', True)
    ]:
      if match := re.match(pattern, lines):
        attr_type = match.group(2).strip()
        attribute_info = {
          "name": self.current_comment.get('name', match.group(1)),
          "func": match.group(1),
          "value_type": attr_type,
          "is_static": is_static
        }

    # 加入当前类数据
    self.current_class['attributes'].append(attribute_info)

  # 解析成员函数信息
  # 期望输入：
  #  virtual scoped_refptr<xx> yy(ExceptionState& exception_state) = 0;
  #  static scoped_refptr<zz> New(ExecutionContext* execution_context,const base::String& name,uint32_t size,ExceptionState& exception_state);
  def process_member(self, lines):
    pattern = r"""
        ^\s*
        (?:virtual\s+)?       # 可选的virtual关键字
        (static\s+)?         # 捕获static关键字
        ((?:[^$\s]|<[^>]*>)+) # 返回值类型(处理嵌套模板)
        \s+
        (\w+)                # 函数名
        \(
        ([^$]*)             # 全部参数
        \)
        (?:\s*=\s*0)?        # 可选的纯虚函数标记
        \s*;\s*$
    """

    match = re.search(pattern, lines, re.VERBOSE)
    if match:
      is_static = bool(match.group(1))
      return_type = match.group(2).strip()
      func_name = match.group(3)

      raw_params = match.group(4)
      params = re.sub(r'\b(?:ExecutionContext\*|ExceptionState&)\s*\w+\s*,?\s*', '', raw_params)
      params = params.strip().rstrip(',')  # 清理多余的逗号

      params_list = []
      for param_item in params.split(','):
        match = re.match(r"^(.*?)\s+([a-zA-Z_]\w*)$", param_item.strip())
        if match:
          p_type = match.group(1).strip()
          p_name = match.group(2).strip()
        else:
          continue

        # 检查声明注释
        optional_params = {}
        if 'optional' in self.current_comment:
          opts = self.current_comment['optional']
          opts = [opts] if not isinstance(opts, list) else opts
          for opt in opts:
            if '=' in opt:
              k, v = opt.split('=', 1)
              optional_params[k.strip()] = v.strip()

        # 过滤掉不需要的参数类型
        if p_type not in ["ExceptionState", "ExecutionContext"]:
          params_list.append({
            "name": p_name,
            "type": p_type,
            "is_optional": p_name.strip() in optional_params,
            "default_value": optional_params.get(p_name.strip(), None),
          })

      # 寻找重载函数
      method_name = self.current_comment.get("name", None)
      method_func = func_name

      # 是否存在重载函数
      existing_overload = next(
        (m for m in self.current_class['methods']
         if m['name'] == method_name
         and m['func'] == method_func
         and m['is_static'] == is_static),
        None
      )

      if existing_overload:
        existing_overload['params'].append(params_list)
        return

      # 添加到类数据
      self.current_class['methods'].append({
        "name": method_name,
        "func": method_func,
        "is_static": is_static,
        "return_type": return_type,
        "params": [params_list],
      })

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
    dependency = [self.current_class["class"]]

    # 查找顺序：
    #  结构体 -> 属性 -> 方法

    # 结构体遍历
    structs_data = self.current_class["structs"]
    for struct in structs_data:
      for member in struct["members"]:
        dependency.append(member["type"])

    # 属性遍历
    attributes_data = self.current_class["attributes"]
    for attribute in attributes_data:
      dependency.append(attribute["value_type"])

    # 方法遍历
    methods_data = self.current_class["methods"]
    for method in methods_data:
      dependency.append(method["return_type"])
      for overload in method["params"]:
        for param in overload:
          dependency.append(param["type"])

    # 去重
    dependency = list(set(dependency))

    # 去除基础数据类型
    base_types = (
      "int8_t",
      "uint8_t",
      "int16_t",
      "uint16_t",
      "int32_t",
      "uint32_t",
      "int64_t",
      "uint64_t",
      "void",
      "bool",
      "float",
      "double",
      "base::String",
    )

    result = []
    for dep in dependency:
      is_base_type = False
      for ty in base_types:
        if dep.find(ty) != -1:
          is_base_type = True
      if not is_base_type:
        pattern = r"scoped_refptr<([^>]+)>"
        match = re.search(pattern, dep)
        if match:
          result.append(match.group(1))

    # 添加到当前类数据
    self.current_class["dependency"] = result

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

// IDL generator format:
// Inhert: refcounted only.
// Interface reference: RGSS Reference
/*--urge(name:Bitmap)--*/
class URGE_RUNTIME_API Bitmap : public base::RefCounted<Bitmap> {
 public:
  virtual ~Bitmap() = default;

  /*--urge(name:Size)--*/
  struct Size {
    uint32_t width;
    uint32_t height;
  };
  
  /*--urge(name:CreateInfo)--*/
  struct CreateInfo {
    uint32_t test;
    uint32_t id = 0;
    base::String filename = "null";
    std::optional<Size> size;
  };

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
