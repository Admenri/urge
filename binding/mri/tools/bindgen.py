# Copyright 2018-2025 Admenri.
# Use of this source code is governed by a BSD - style license that can be
# found in the LICENSE file.

import sys
import re

sys.path.append(".")
from binding.build import api_parser

autogen_header = """
// Copyright (c) 2018-2025 Admenri. All rights reserved.
//
// ---------------------------------------------------------------------------
//
//  此文件通过URGE引擎自带的MRI绑定生成器自动生成，请勿直接修改本文件的内容。
//  生成器位置：binding/mri/tools/bindgen.py
//
"""

class MriBindingGen:
  def __init__(self):
    # 当前将要生成类的全部数据
    self.class_data = {}
    # 当前正在解析的头文件名
    self.header_filename = ""

  # 生成头文件
  def generate_header(self):
    # C++类名
    klass_type = self.class_data['class']
    # 是否为模块
    is_module = self.class_data['is_module']
    # 当前类的头文件
    header_file = self.class_data['filename']

    # 版权+注意事项声明
    content = autogen_header

    # 防冲突宏
    content += f"#ifndef BINDING_MRI_AUTOGEN_{klass_type.upper()}_BINDING_H_\n"
    content += f"#define BINDING_MRI_AUTOGEN_{klass_type.upper()}_BINDING_H_\n\n"

    # 引入公共库
    content += "#include \"binding/mri/mri_util.h\"\n"

    # 引入引擎接口
    content += f"#include \"content/public/{header_file}\"\n"

    # 开始声明内容
    content += "\nnamespace binding {\n\n"

    # 定义导出类数据
    if not is_module:
      content += f"MRI_DECLARE_DATATYPE({klass_type});\n\n"
    content += f"void Init{klass_type}Binding();\n\n"

    content += "}\n"
    content += f"\n#endif // !BINDING_MRI_AUTOGEN_{klass_type.upper()}_BINDING_H_\n"

    return content, f"autogen_{klass_type.lower()}_binding.h"

  # 生成 Ruby 函数声明部分
  def generate_body_declare(self):
    # C++类名
    klass_type = self.class_data['class']
    # Ruby类名
    klass_name = self.class_data['name']
    # 是否模块
    is_module = self.class_data['is_module']
    # 是否可比较
    is_comparable = self.class_data['is_comparable']
    # 是否可序列化
    is_serializable = self.class_data['is_serializable']

    # 定义函数体
    content = f"void Init{klass_type}Binding() {{\n"

    # 定义类
    if not is_module:
      # 类定义头 
      content += f"VALUE klass = rb_define_class(\"{klass_name}\", rb_cObject);\n"
      content += f"rb_define_alloc_func(klass, MriClassAllocate<&k{klass_type}DataType>);\n"
      # 由于引擎每次取出对象的 Ruby ObjectID 都不一样，这里需要为每个类重载比较函数
      content += "MriDefineMethod(klass, \"engine_id\", MriGetEngineID);\n"
      content += f"MRI_DECLARE_OBJECT_COMPARE(klass, {klass_name});\n"
    else:
      content += f"VALUE klass = rb_define_module(\"{klass_name}\");\n"

    # 类特性定义
    if is_serializable:
      content += f"MriInitSerializableBinding<content::{klass_type}>(klass);\n"

    # 枚举常量定义
    enums = self.class_data['enums']
    for enum in enums:
      for const in enum["constants"]:
        enum_name = enum["name"]
        enum_type = enum["type"]
        content += f"rb_const_set(klass, rb_intern(\"{enum_name}\"), INT2NUM(content::{klass_type}::{enum_type}::{const}));\n"
      content += "\n"

    # 属性定义
    attributes = self.class_data['attributes']
    for attr in attributes:
      # 属性 Ruby 名
      attribute_name = attr['name']
      # 属性 C++ 类型
      attribute_type = attr['func']
      # 根据类的归属选择定义宏
      if not is_module:
        if not attr['is_static']:
          # 类属性
          declare_type = "MRI_DECLARE_ATTRIBUTE"
        else:
          # 静态类属性
          declare_type = "MRI_DECLARE_CLASS_ATTRIBUTE"
      else:
        # 模块属性
        declare_type = "MRI_DECLARE_MODULE_ATTRIBUTE"
      # 添加属性
      content += f"{declare_type}(klass, \"{attribute_name}\", {klass_name}, {attribute_type});\n"

    # 函数定义
    methods = self.class_data['methods']
    for method in methods:
      # 方法的 Ruby 名称
      method_name = method['name']
      # 方法的 C++ 名称
      method_func = method['func']

      # 根据类的归属选择定义宏
      if not is_module:
        if not method['is_static']:
          # 类属性
          declare_type = "MriDefineMethod"
        else:
          # 静态类属性
          declare_type = "MriDefineClassMethod"
      else:
        # 模块属性
        declare_type = "MriDefineModuleFunction"

      # 特殊方法
      if method_name == "initialize":
        declare_type = "MriDefineMethod"
      if method_name == "initialize_copy":
        declare_type = "MriDefineMethod"

      # 添加方法声明
      content += f"{declare_type}(klass, \"{method_name}\", {klass_name}_{method_func});\n"

    # 函数体结尾
    content += "}\n"
    return content

  # 判断一个类型是否为 enum
  def is_type_enum(self, typename):
    deps = self.class_data['enums']
    data = []
    for dep in deps:
      data.append(dep['type'])
    return typename in data

  # 判断一个类型是否为 struct
  def is_type_struct(self, typename):
    deps = self.class_data['structs']
    data = []
    for dep in deps:
      data.append(dep['type'])
    return typename in data

  # 生成一个函数，将 Ruby 的 Hash 转为 C++ 的 Struct
  # 函数名：*** RBHASH2****(VALUE)
  def convert_hash_to_struct(self, struct_name):
    # 生成一个专用函数，将本类的结构体从 Ruby 数据转为 C++ 数据
    content = f"static content::{self.class_data['class']}::{struct_name} RBHASH2{struct_name}(VALUE rb_hash) {{\n"
    # 创建一个局部变量
    content += f"content::{self.class_data['class']}::{struct_name} result;\n\n"

    # 从当前类的结构体中取出对应的数据
    structs = self.class_data['structs']
    target_struct = next((x for x in structs if x['type'] == struct_name), None)
    for member in target_struct['members']:
      # 成员变量名称
      member_name = member['name']
      # 成员类型
      member_type = member['type']

      # 在 Hash 中寻找指定成员
      content += f"VALUE {member_name} = rb_hash_lookup(rb_hash, ID2SYM(rb_intern(\"{member_name}\")));\n"

      # 通过成员变量类型确认转换函数
      convert_func = ""
      if member_type.startswith("uint32_t"):
        convert_func = f"NUM2UINT({member_name})"
      elif member_type.startswith("int64_t"):
        convert_func = f"NUM2LL({member_name})"
      elif member_type.startswith("uint64_t"):
        convert_func = f"NUM2ULL({member_name})"
      elif member_type.startswith("int") or member_type.startswith("uint") or self.is_type_enum(member_type):
        if self.is_type_enum(member_type):
          convert_func = f"(content::{self.class_data['class']}::{member_type})NUM2INT({member_name})"
        else:
          convert_func = f"NUM2INT({member_name})"
      elif member_type.startswith("float"):
        convert_func = f"RFLOAT_VALUE({member_name})"
      elif member_type.startswith("double"):
        convert_func = f"RFLOAT_VALUE({member_name})"
      elif member_type.startswith("bool"):
        convert_func = f"MRI_FROM_BOOL({member_name})"
      elif member_type.startswith("base::String"):
        convert_func = f"MRI_FROM_STRING({member_name})"
      elif member_type.startswith("scoped_refptr"):
        match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', param_type)
        decay_type = match.group(1)
        convert_func = f"MriCheckStructData<content::{decay_type}>({member_name}, k{decay_type}DataType)"
      elif member_type.startswith("std::optional"):
        match = re.search(r'(?:const\s+)?std::optional\s*<\s*([^\s>]+)\s*>(?:&\s*)?', member_type)
        decay_type = match.group(1)
        convert_func = f"RBHASH2{decay_type}({member_name})"
      elif member_type.startswith("base::Vector"):
        match = re.search(r'base::Vector<(.+)>', member_type)
        decay_type = match.group(1)

        # 萃取 base::Vector 中的类型并使用对应的转换
        if decay_type.startswith("scoped_refptr"):
          match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', decay_type)
          match_type = match.group(1)
          convert_func = f"RBARRAY2CXX<content::{match_type}>({member_name}, k{match_type}DataType)"
        elif self.is_type_struct(decay_type):
          convert_func = f"RBARRAY2CXX<content::{self.class_data['class']}::{decay_type}>({member_name}, RBHASH2{decay_type})"
        elif self.is_type_enum(decay_type):
          convert_func = f"RBARRAY2CXX_CONST<content::{self.class_data['class']}::{decay_type}>({member_name})"
        else:
          convert_func = f"RBARRAY2CXX<{decay_type}>({member_name})"

      content += f"if ({member_name} != Qnil)\n"
      content += f"  result.{member_name} = {convert_func};\n"

    content += "\nreturn result;\n"
    content += "}\n"
    return content

  # 生成一个函数，将 C++ 的 Struct 转为 Ruby 的 Hash
  # 函数名：VALUE ****2RBHASH(const ***&)
  def convert_struct_to_hash(self, struct_name):
    # 生成一个专用函数，将本类的结构体从 C++ 数据转为 Ruby 数据
    content = f"static VALUE {struct_name}2RBHASH(const std::optional<content::{self.class_data['class']}::{struct_name}>& opt_cxx_obj) {{\n"
    content += "if (!opt_cxx_obj.has_value())\nreturn Qnil;\n"
    content += "auto& cxx_obj = *opt_cxx_obj;\n"

    # 创建一个局部变量
    content += f"VALUE result = rb_hash_new();\n\n"

    # 从当前类的结构体中取出对应的数据
    structs = self.class_data['structs']
    target_struct = next((x for x in structs if x['type'] == struct_name), None)
    for member in target_struct['members']:
      # 成员变量名称
      member_name = member['name']
      # 成员类型
      member_type = member['type']

      type_mapping = {
        "int8_t": "INT2NUM",
        "uint8_t": "INT2NUM",
        "int16_t": "INT2NUM",
        "uint16_t": "INT2NUM",
        "int32_t": "INT2NUM",
        "uint32_t": "UINT2NUM",
        "int64_t": "LL2NUM",
        "uint64_t": "ULL2NUM",
        "bool": "MRI_BOOL_VALUE",
        "float": "DBL2NUM",
        "double": "DBL2NUM",
        "base::String": "MRI_STRING_VALUE",
      }

      convert_func = ""
      if self.is_type_enum(member_type):
        convert_func = f"INT2NUM(cxx_obj.{member_name})"
      elif member_type.startswith("scoped_refptr"):
        match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', member_type)
        decay_type = match.group(1)
        convert_func = f"MriWrapObject<content::{decay_type}>(cxx_obj.{member_name}, k{decay_type}DataType)"
      elif member_type.startswith("std::optional"):
        match = re.search(r'(?:const\s+)?std::optional\s*<\s*([^\s>]+)\s*>(?:&\s*)?', member_type)
        decay_type = match.group(1)
        convert_func = f"{decay_type}2RBHASH(cxx_obj.{member_name})"
      elif member_type.startswith("base::Vector"):
        match = re.search(r'base::Vector<(.+)>', member_type)
        decay_type = match.group(1)

        # 萃取 base::Vector 中的类型并使用对应的转换
        if decay_type.startswith("scoped_refptr"):
          match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', decay_type)
          match_type = match.group(1)
          convert_func = f"CXX2RBARRAY<content::{match_type}>(cxx_obj.{member_name}, k{match_type}DataType)"
        elif self.is_type_struct(decay_type):
          convert_func = f"CXX2RBARRAY<content::{self.class_data['class']}::{decay_type}>(cxx_obj.{member_name}, {decay_type}2RBHASH)"
        elif self.is_type_enum(decay_type):
          convert_func = f"CXX2RBARRAY<content::{self.class_data['class']}::{decay_type}>(cxx_obj.{member_name})"
        else:
          convert_func = f"CXX2RBARRAY<{decay_type}>(cxx_obj.{member_name})"

      elif type_mapping.get(member_type, None) is not None:
        convert_func += f"{type_mapping.get(member_type)}(cxx_obj.{member_name})"

      # 设置数据到 Ruby Hash
      content += f"rb_hash_aset(result, ID2SYM(rb_intern(\"{member_name}\")), {convert_func});\n"

    content += "\nreturn result;\n"
    content += "}\n"
    return content

  # 函数定义主体部分
  def generate_body_definition(self):
    # C++类名
    klass_type = self.class_data['class']
    # Ruby类名
    klass_name = self.class_data['name']
    # 是否模块
    is_module = self.class_data['is_module']

    # 这里将属性视为方法，因为在 RGSS 中就是使用方法来模拟的属性
    methods = self.class_data['methods']
    attributes = self.class_data['attributes']
    for attr in attributes:
      # 属性 Ruby 名
      attribute_name = attr['name']
      # 属性 C++ 名
      attribute_func = attr['func']
      # 是否静态
      is_static = attr['is_static']
      # 属性类型
      attribute_type = attr['value_type']

      # 定义 getter 方法模版
      attr_getter_method = {
        "name": attribute_name,
        "func": f"Get_{attribute_func}",
        "is_static": is_static,
        "return_type": attribute_type,
        "params": [[]],
      }

      # 定义 setter 方法模版
      attr_setter_method = {
        "name": f"{attribute_name}=",
        "func": f"Put_{attribute_func}",
        "is_static": is_static,
        "return_type": "void",
        "params": [
          [
            {
              "name": "value",
              "type": attribute_type,
              "is_optional": False,
              "default_value": None,
            }
          ]
        ],
      }

      # 添加到方法解析列表
      methods.append(attr_getter_method)
      methods.append(attr_setter_method)

    # 开始生成函数体内容
    content = ""

    # 解析方法函数本体
    for method in methods:
      # C++ 函数名
      method_func = method['func']
      # 重载列表
      overloads = method['params']
      # 是否静态
      is_static = method['is_static']

      # 函数头部
      content += f"MRI_METHOD({klass_name}_{method_func}) {{\n"

      # 如果有重载，根据参数数量决定是哪个函数
      if len(overloads) > 1:
        content += "switch (argc) {\n"

      # 解析重载列表
      for params in overloads:
        # 当前重载的参数数量
        params_count = len(params)

        # 如果有重载，设置参数数量分支
        if len(overloads) > 1:
          content += f"case {params_count}: {{\n"

        parse_template = ""
        parse_reference = ""
        convert_suffix = ""
        has_optional = False
        for param in params:
          param_name = param['name']
          param_type = param['type']
          param_is_optional = param['is_optional']
          param_default_value = param['default_value']
          ruby_type = "VALUE"

          # 是否开始 optional 模式
          if not has_optional and param_is_optional:
            parse_template += "|"
          has_optional = param_is_optional

          # 识别 C++ 类型对应的 Ruby 类型
          if param_type == "uint32_t":
            parse_template += "u"
            ruby_type = "uint32_t"
          elif param_type == "int64_t":
            parse_template += "l"
            ruby_type = "int64_t"
          elif param_type == "uint64_t":
            parse_template += "p"
            ruby_type = "uint64_t"
          elif param_type == "int32_t" or param_type.startswith("int") or param_type.startswith("uint") or self.is_type_enum(param_type):
            parse_template += "i"
            ruby_type = "int32_t"
          elif param_type == "bool":
            parse_template += "b"
            ruby_type = "bool"
          elif param_type == "float":
            parse_template += "f"
            ruby_type = "double"
          elif param_type == "double":
            parse_template += "f"
            ruby_type = "double"
          elif param_type.startswith("const base::String&") or param_type.startswith("base::String"):
            parse_template += "s"
            ruby_type = "base::String"
          elif param_type.startswith("scoped_refptr"):
            match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', param_type)
            decay_type = match.group(1)
            parse_template += "o"
            ruby_type = "VALUE"
            convert_suffix += f"auto {param_name} = MriCheckStructData<content::{decay_type}>({param_name}_obj, k{decay_type}DataType);\n"
            param_name += "_obj"
          elif param_type.startswith("std::optional") or param_type.startswith("const std::optional"):
            match = re.search(r'(?:const\s+)?std::optional\s*<\s*([^\s>]+)\s*>(?:&\s*)?', param_type)
            decay_type = match.group(1)
            parse_template += "o"
            ruby_type = "VALUE"
            convert_suffix += f"auto {param_name} = RBHASH2{decay_type}(rb_check_hash_type({param_name}_obj));\n"
            param_name += "_obj"
          elif param_type.startswith("const base::Vector") or param_type.startswith("base::Vector"):
            parse_template += "o"
            ruby_type = "VALUE"

            match = re.search(r'base::Vector<(.+)>', param_type)
            decay_type = match.group(1)

            # 萃取 base::Vector 中的类型并使用对应的转换
            if decay_type.startswith("scoped_refptr"):
              match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', decay_type)
              match_type = match.group(1)
              convert_func = f"RBARRAY2CXX<content::{match_type}>({param_name}_ary, k{match_type}DataType)"
            elif self.is_type_struct(decay_type):
              convert_func = f"RBARRAY2CXX<content::{klass_type}::{decay_type}>({param_name}_ary, RBHASH2{decay_type})"
            elif self.is_type_enum(decay_type):
              convert_func = f"RBARRAY2CXX_CONST<content::{klass_type}::{decay_type}>({param_name}_ary)"
            else:
              convert_func = f"RBARRAY2CXX<{decay_type}>({param_name}_ary)"

            convert_suffix += f"auto {param_name} = {convert_func};\n"
            param_name += "_ary"

          # 生成引用参数列表
          parse_reference += f"&{param_name}, "

          # 生成参数变量定义
          # 是否有默认值
          if param_is_optional:
            if ruby_type == "VALUE":
              param_default_value = "Qnil"
            content += f"{ruby_type} {param_name} = {param_default_value};\n"
          else:
            content += f"{ruby_type} {param_name};\n"

        # 去除引用右边多余逗号
        parse_reference = parse_reference.strip().rstrip(',')

        # 生成参数解析
        if parse_reference != "":
          content += f"MriParseArgsTo(argc, argv, \"{parse_template}\", {parse_reference});\n"

        # 添加对象转换
        content += convert_suffix + "\n"

        # 获取当前对象
        if not is_static:
          if is_module:
            content += f"scoped_refptr self_obj = MriGetGlobalModules()->{klass_name};\n"
          else:
            content += f"scoped_refptr self_obj = MriGetStructData<content::{klass_type}>(self);\n"

        # 生成调用参数列表
        calling_parameters = ""
        for param in params:
          param_name = param['name']
          param_type = param['type']
          if self.is_type_enum(param_type):
            calling_parameters += f"(content::{klass_type}::{param_type})"
          elif param_type == "float":
            calling_parameters += "(float)"
          calling_parameters += f"{param_name},"

        # 设置返回值前缀
        return_type = method['return_type']
        return_suffix = ""
        if return_type != "void":
          return_suffix = "auto _return_value = "

        # 调用函数并获得返回值
        content += "content::ExceptionState exception_state;\n"
        if not is_static:
          content += f"{return_suffix}self_obj->{method_func}({calling_parameters} exception_state);\n"
        else:
          content += f"{return_suffix}content::{klass_type}::{method_func}(MriGetCurrentContext(), {calling_parameters} exception_state);\n"
        content += "MriProcessException(exception_state);\n"

        # 处理返回值
        if return_type != "void":
          if method_func == "New" or method_func == "Copy":
            # 特殊返回值处理
            content += "_return_value->AddRef();\n"
            content += "MriSetStructData(self, _return_value.get());\n"
            content += "return self;\n"
          else:
            # 普通返回值处理
            type_mapping = {
              "int8_t": "INT2NUM",
              "uint8_t": "INT2NUM",
              "int16_t": "INT2NUM",
              "uint16_t": "INT2NUM",
              "int32_t": "INT2NUM",
              "uint32_t": "UINT2NUM",
              "int64_t": "LL2NUM",
              "uint64_t": "ULL2NUM",
              "bool": "MRI_BOOL_VALUE",
              "float": "DBL2NUM",
              "double": "DBL2NUM",
              "base::String": "MRI_STRING_VALUE",
            }

            if self.is_type_enum(return_type):
              content += f"VALUE _result = INT2NUM(_return_value);\n"
            elif return_type.startswith("scoped_refptr"):
              match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', return_type)
              decay_type = match.group(1)
              content += f"VALUE _result = MriWrapObject<content::{decay_type}>(_return_value, k{decay_type}DataType);\n"
            elif return_type.startswith("std::optional"):
              match = re.search(r'(?:const\s+)?std::optional\s*<\s*([^\s>]+)\s*>(?:&\s*)?', return_type)
              decay_type = match.group(1)
              content += f"VALUE _result = {decay_type}2RBHASH(_return_value);\n"
            elif return_type.startswith("base::Vector"):
              match = re.search(r'base::Vector<(.+)>', return_type)
              decay_type = match.group(1)

              # 萃取 base::Vector 中的类型并使用对应的转换
              if decay_type.startswith("scoped_refptr"):
                match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', decay_type)
                match_type = match.group(1)
                convert_func = f"CXX2RBARRAY<content::{match_type}>(_return_value, k{match_type}DataType)"
              elif self.is_type_struct(decay_type):
                convert_func = f"CXX2RBARRAY<{decay_type}>(_return_value, {decay_type}2RBHASH)"
              elif self.is_type_enum(decay_type):
                convert_func = f"CXX2RBARRAY<int32_t>(_return_value)"
              else:
                convert_func = f"CXX2RBARRAY<{decay_type}>(_return_value)"

              content += f"VALUE _result = {convert_func};\n"
            elif type_mapping.get(return_type, None) is not None:
              content += f"VALUE _result = {type_mapping.get(return_type)}(_return_value);\n"

            content += "return _result;\n"
        else:
          # 没有返回值
          content += "return Qnil;\n"

        # 如果有重载，设置分支结束语句
        if len(overloads) > 1:
          content += "}\n"

      # 如果有重载，参数数量不对时抛出异常
      if len(overloads) > 1:
        content += "default:\n"
        content += "rb_raise(rb_eArgError, \"failed to determine overload method. (count: %d)\", argc);\n"
        content += "return Qnil;\n"
        content += "}\n"

      # 函数尾部
      content += "}\n\n"

    return content

  # 生成源文件
  def generate_body(self):
    # C++类名
    klass_type = self.class_data['class']
    # Ruby类名
    klass_name = self.class_data['name']
    # 是否模块
    is_module = self.class_data['is_module']
    # 是否可比较
    is_comparable = self.class_data['is_comparable']

    # 版权+注意事项声明
    content = autogen_header

    # 包含头文件
    content += f"\n#include \"binding/mri/autogen_{klass_type.lower()}_binding.h\"\n\n"

    # 添加依赖头文件
    for dep in self.class_data['dependency']:
      content += f"#include \"binding/mri/autogen_{dep.lower()}_binding.h\"\n"

    # 开始源文件内容
    content += "\nnamespace binding {\n"

    if not is_module:
      # 定义 Ruby 数据结构（仅对类有效）
      content += f"MRI_DEFINE_DATATYPE_REF({klass_name}, \"{klass_name}\", content::{klass_type});\n"

      # 定义 Ruby 类特性
      if is_comparable:
        content += f"MRI_OBJECT_ID_COMPARE_CUSTOM({klass_name});\n"
      else:
        content += f"MRI_OBJECT_ID_COMPARE({klass_name});\n"

    content += "\n" * 2

    # 添加 Hash 转为 Struct 的函数部分
    for struct in self.class_data['structs']:
      content += self.convert_hash_to_struct(struct['type'])
      content += self.convert_struct_to_hash(struct['type'])

    # 添加定义部分
    content += self.generate_body_definition()

    # 添加声明部分
    content += self.generate_body_declare()

    # 内容
    content += "} // namespace binding \n"

    return content, f"autogen_{klass_type.lower()}_binding.cc"

  # 设置要解析的接口头文件内容
  def setup(self, class_info):
    self.class_data = class_info

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
// Inherit: refcounted only.
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
                                   const base::Vector<scoped_refptr<Test>> test,
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

  parser = api_parser.APIParser()
  parser.parse(cpp_code)

  for item in parser.classes:
    gen = MriBindingGen()
    gen.setup(item)
    print("====================header=====================")
    s, h = gen.generate_header()
    print(s)
    print("=====================body=====================")
    s, h = gen.generate_body()
    print(s)
    print("=====================end=======================")
