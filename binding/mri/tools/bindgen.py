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
    # 全部类的信息（用于获取枚举，结构体信息）
    self.global_classes = {}
    # 当前将要生成类的全部数据
    self.class_data = {}
    # 当前正在解析的头文件名
    self.header_filename = ""

  # 生成头文件
  def generate_header(self):
    # C++类名
    klass_type = self.class_data['native_name']
    # 当前类的头文件
    header_file = self.class_data['filename']
    # 当前对象的类型判断
    object_type = self.class_data['type']

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

    # 是否为模块
    rb_data_type = (object_type == 'class' and not self.class_data['is_module'])
    rb_data_type |= object_type == 'struct'
    if rb_data_type:
      content += f"MRI_DECLARE_DATATYPE({klass_type});\n\n"
    content += f"void Init{klass_type}Binding();\n\n"

    content += "}\n"
    content += f"\n#endif // !BINDING_MRI_AUTOGEN_{klass_type.upper()}_BINDING_H_\n"

    return content, f"autogen_{klass_type.lower()}_binding.h", f"Init{klass_type}Binding()"

  # 生成 Ruby 函数声明部分
  def generate_body_declare(self):
    # 当前对象的类型判断
    object_type = self.class_data['type']
    if object_type == 'class':
      return self.create_class_declaration()
    if object_type == 'struct':
      return self.create_struct_declaration()

  def create_struct_declaration(self):
    # C++类名
    klass_type = self.class_data['native_name']
    # Ruby类名
    klass_name = self.class_data['binding_name']
    # 成员列表
    struct_members = self.class_data['members']

    # 定义函数体
    content = f"void Init{klass_type}Binding() {{\n"

    # 类定义头
    content += f"VALUE klass = rb_define_class(\"{klass_name}\", rb_cObject);\n"
    content += f"rb_define_alloc_func(klass, MriClassAllocate<&k{klass_type}DataType>);\n"
    # 由于引擎每次取出对象的 Ruby ObjectID 都不一样，这里需要为每个类重载比较函数
    content += "MriDefineMethod(klass, \"engine_id\", MriGetEngineID);\n"

    # 初始化方法
    content += f"MriDefineMethod(klass, \"initialize\", MriCommonStructNew<content::{klass_type}>);\n"

    # 定义成员访问
    for member in struct_members:
      member_name = member['native_name']
      content += f"MRI_DECLARE_ATTRIBUTE(klass, \"{member_name}\", {klass_name}, {member_name});\n"

    # 函数体结尾
    content += "\n}\n"
    return content

  def create_class_declaration(self):
    # C++类名
    klass_type = self.class_data['native_name']
    # Ruby类名
    klass_name = self.class_data['binding_name']
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
        enum_name = enum["binding_name"]
        enum_type = enum["native_name"]
        content += f"rb_const_set(klass, rb_intern(\"{const}\"), INT2NUM(content::{klass_type}::{enum_type}::{const}));\n"
      content += "\n"

    # 属性定义
    attributes = self.class_data['attributes']
    for attr in attributes:
      # 属性 Ruby 名
      attribute_name = attr['binding_name']
      # 属性 C++ 类型
      attribute_type = attr['native_name']
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
      method_name = method['binding_name']
      # 方法的 C++ 名称
      method_func = method['native_name']

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

  @staticmethod
  def parse_namespace_element(varname):
    parts = varname.split('::', 1)  # 最多分割一次
    if len(parts) == 2:
      left, right = parts
      return True, left, right
    return False, None, None

  # 判断一个类型是否为 enum
  def is_type_enum(self, typename):
    is_other_domain, domain, varname = self.parse_namespace_element(typename)
    if is_other_domain:
      for klass in self.global_classes:
        if klass['native_name'] == domain:
          for enum in klass['enums']:
            if enum['native_name'] == varname:
              return True

    deps = self.class_data.get('enums', None)
    if deps is None:
      return False

    data = []
    for dep in deps:
      data.append(dep['native_name'])
    return typename in data

  # 函数定义主体部分
  def generate_body_definition(self):
    # 当前对象的类型判断
    object_type = self.class_data['type']
    if object_type == 'class':
      return self.create_class_content()
    if object_type == 'struct':
      return self.create_struct_content()

  # 生成结构体的数据
  def create_struct_content(self):
    # C++类名
    klass_type = self.class_data['native_name']
    # Ruby类名
    klass_name = self.class_data['binding_name']

    # 开始生成内容
    content = ""

    for member in self.class_data['members']:
      # 成员函数名称
      member_name = member['native_name']
      # 成员类型
      type_raw = member['type_raw']
      type_detail = member['type_detail']

      # ==== 取属性 ====
      # 函数头部
      content += f"MRI_METHOD({klass_name}_Get_{member_name}) {{\n"
      # 检查函数数量
      content += "if (argc > 0)\nrb_raise(rb_eArgError, \"Too many arguments for struct getter.\");\n"
      # 获取 self
      content += f"scoped_refptr self_obj = MriGetStructData<content::{klass_type}>(self);\n"

      # 基础类型转换
      basic_type_convert = {
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
        "std::string": "MRI_STRING_VALUE",
      }

      if type_raw in basic_type_convert:
        content += f"VALUE result = {basic_type_convert.get(type_raw)}(self_obj->{member_name});\n"
      elif self.is_type_enum(type_raw):
        content += f"VALUE result = INT2NUM(self_obj->{member_name});\n"
      elif len(type_detail['containers']) == 1 and type_detail['containers'][0] == "scoped_refptr":
        root_type = type_detail['root_type']
        content += f"VALUE result = MriWrapObject<content::{root_type}>(self_obj->{member_name}, k{root_type}DataType);\n"
      elif len(type_detail['containers']) >= 1 and type_detail['containers'][0] == "std::vector":
        root_type = type_detail['root_type']
        second_container = type_detail['containers']
        second_container = "" if len(second_container) == 1 else second_container[1]

        # 萃取 std::vector 中的类型并使用对应的转换
        if second_container.startswith("scoped_refptr"):
          content += f"VALUE result = CXX2RBARRAY<content::{root_type}>(self_obj->{member_name}, k{root_type}DataType);\n"
        elif self.is_type_enum(root_type):
          content += f"VALUE result = CXX2RBARRAY<content::{root_type}>(self_obj->{member_name});\n"
        else:
          content += f"VALUE result = CXX2RBARRAY<{root_type}>(self_obj->{member_name});\n"

      # 返回值
      content += "return result;\n"
      # 函数尾部
      content += "}\n\n"

      # ==== 置属性 ====
      # 函数头部
      content += f"MRI_METHOD({klass_name}_Put_{member_name}) {{\n"
      # 检查函数数量
      content += "if (argc > 1)\nrb_raise(rb_eArgError, \"Too many arguments for struct setter.\");\n"
      # 获取 self
      content += f"scoped_refptr self_obj = MriGetStructData<content::{klass_type}>(self);\n"

      # 基础类型转换
      basic_type_convert = {
        "int8_t": "NUM2INT",
        "uint8_t": "NUM2INT",
        "int16_t": "NUM2INT",
        "uint16_t": "NUM2INT",
        "int32_t": "NUM2INT",
        "uint32_t": "NUM2UINT",
        "int64_t": "NUM2LL",
        "uint64_t": "NUM2ULL",
        "bool": "MRI_FROM_BOOL",
        "float": "NUM2DBL",
        "double": "NUM2DBL",
        "std::string": "MRI_FROM_STRING",
      }

      if type_raw in basic_type_convert:
        content += f"self_obj->{member_name} = {basic_type_convert.get(type_raw)}(argv[0]);\n"
      elif self.is_type_enum(type_raw):
        content += f"self_obj->{member_name} = (content::{type_raw})NUM2INT(argv[0]);\n"
      elif len(type_detail['containers']) == 1 and type_detail['containers'][0] == "scoped_refptr":
        root_type = type_detail['root_type']
        content += f"self_obj->{member_name} = MriCheckStructData<content::{root_type}>(argv[0], k{root_type}DataType);\n"
      elif len(type_detail['containers']) >= 1 and type_detail['containers'][0] == "std::vector":
        root_type = type_detail['root_type']
        second_container = type_detail['containers']
        second_container = "" if len(second_container) == 1 else second_container[1]

        # 萃取 std::vector 中的类型并使用对应的转换
        if second_container.startswith("scoped_refptr"):
          content += f"self_obj->{member_name} = RBARRAY2CXX<content::{root_type}>(argv[0], k{root_type}DataType);\n"
        elif self.is_type_enum(root_type):
          content += f"self_obj->{member_name} = RBARRAY2CXX<content::{root_type}>(argv[0]);\n"
        else:
          content += f"self_obj->{member_name} = RBARRAY2CXX<{root_type}>(argv[0]);\n"

      # 返回值
      content += "return self;\n"
      # 函数尾部
      content += "}\n\n"

    return content

  # 生成类的数据
  def create_class_content(self):
    # C++类名
    klass_type = self.class_data['native_name']
    # Ruby类名
    klass_name = self.class_data['binding_name']
    # 是否模块
    is_module = self.class_data['is_module']

    # 这里将属性视为方法，因为在 RGSS 中就是使用方法来模拟的属性
    methods = self.class_data['methods']
    attributes = self.class_data['attributes']
    for attr in attributes:
      # 属性 Ruby 名
      attribute_name = attr['binding_name']
      # 属性 C++ 名
      attribute_func = attr['native_name']
      # 是否静态
      is_static = attr['is_static']
      # 属性类型
      attribute_type = attr['type_raw']
      attribute_type_detail = attr['type_detail']

      # 定义 getter 方法模版
      attr_getter_method = {
        "binding_name": attribute_name,
        "native_name": f"Get_{attribute_func}",
        "is_static": is_static,
        "return_type_raw": attribute_type,
        "return_type_detail": attribute_type_detail,
        "params": [[]],
      }

      # 定义 setter 方法模版
      attr_setter_method = {
        "binding_name": f"{attribute_name}=",
        "native_name": f"Put_{attribute_func}",
        "is_static": is_static,
        "return_type_raw": "void",
        "return_type_detail": {
          "root_type": "void",
          "containers": [],
        },
        "params": [
          [
            {
              "native_name": "value",
              "type_raw": attribute_type,
              "type_detail": attribute_type_detail,
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
      method_func = method['native_name']
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
          param_name = param['native_name']
          param_type = param['type_raw']
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
          elif param_type.startswith("const std::string&") or param_type.startswith("std::string"):
            parse_template += "s"
            ruby_type = "std::string"
          elif param_type.startswith("scoped_refptr"):
            match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', param_type)
            decay_type = match.group(1)
            parse_template += "o"
            ruby_type = "VALUE"
            convert_suffix += f"auto {param_name} = MriCheckStructData<content::{decay_type}>({param_name}_obj, k{decay_type}DataType);\n"
            param_name += "_obj"
          elif param_type.startswith("void*") or param_type.startswith("const void*"):
            parse_template += "r"
            ruby_type = "void*"
          elif param_type.startswith("const std::vector") or param_type.startswith("std::vector"):
            parse_template += "o"
            ruby_type = "VALUE"

            match = re.search(r'std::vector<(.+)>', param_type)
            decay_type = match.group(1)

            # 萃取 std::vector 中的类型并使用对应的转换
            if decay_type.startswith("scoped_refptr"):
              match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', decay_type)
              match_type = match.group(1)
              convert_func = f"RBARRAY2CXX<content::{match_type}>({param_name}_ary, k{match_type}DataType)"
            elif self.is_type_enum(decay_type):
              convert_func = f"RBARRAY2CXX<content::{klass_type}::{decay_type}>({param_name}_ary)"
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
          param_name = param['native_name']
          param_type = param['type_raw']
          if self.is_type_enum(param_type):
            is_other_domain, domain, varname = self.parse_namespace_element(param_type)
            calling_parameters += f"(content::{domain if is_other_domain else klass_type}::{varname if is_other_domain else param_type})"
          elif param_type == "float":
            calling_parameters += "(float)"
          calling_parameters += f"{param_name},"

        # 设置返回值前缀
        return_type = method['return_type_raw']
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
              "std::string": "MRI_STRING_VALUE",
            }

            if self.is_type_enum(return_type):
              content += f"VALUE _result = INT2NUM(_return_value);\n"
            elif return_type.startswith("scoped_refptr"):
              match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', return_type)
              decay_type = match.group(1)
              content += f"VALUE _result = MriWrapObject<content::{decay_type}>(_return_value, k{decay_type}DataType);\n"
            elif return_type.startswith("std::vector"):
              match = re.search(r'std::vector<(.+)>', return_type)
              decay_type = match.group(1)

              # 萃取 std::vector 中的类型并使用对应的转换
              if decay_type.startswith("scoped_refptr"):
                match = re.search(r'scoped_refptr\s*<\s*([^\s>]+)\s*>', decay_type)
                match_type = match.group(1)
                convert_func = f"CXX2RBARRAY<content::{match_type}>(_return_value, k{match_type}DataType)"
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
    klass_type = self.class_data['native_name']
    # Ruby类名
    klass_name = self.class_data['binding_name']
    # 类型
    current_type = self.class_data['type']

    # 版权+注意事项声明
    content = autogen_header

    # 包含头文件
    content += f"\n#include \"binding/mri/autogen_{klass_type.lower()}_binding.h\"\n\n"

    # 添加依赖头文件
    for dep in self.class_data['dependency']:
      content += f"#include \"binding/mri/autogen_{dep.lower()}_binding.h\"\n"

    # 开始源文件内容
    content += "\nnamespace binding {\n"

    if current_type == 'class':
      # 是否模块
      is_module = self.class_data['is_module']
      # 是否可比较
      is_comparable = self.class_data['is_comparable']

      if not is_module:
        # 定义 Ruby 数据结构（仅对类有效）
        content += f"MRI_DEFINE_DATATYPE_REF({klass_name}, \"{klass_name}\", content::{klass_type});\n"

        # 定义 Ruby 类特性
        if is_comparable:
          content += f"MRI_OBJECT_ID_COMPARE_CUSTOM({klass_name});\n"
        else:
          content += f"MRI_OBJECT_ID_COMPARE({klass_name});\n"
    else:
      # 定义 Ruby 数据结构（仅对结构有效）
      content += f"MRI_DEFINE_DATATYPE_REF({klass_name}, \"{klass_name}\", content::{klass_type});\n"

    content += "\n" * 2

    # 添加定义部分
    content += self.generate_body_definition()

    # 添加声明部分
    content += self.generate_body_declare()

    # 内容
    content += "} // namespace binding \n"

    return content, f"autogen_{klass_type.lower()}_binding.cc"

  # 设置要解析的接口头文件内容
  def setup(self, all_classes, class_info):
    self.global_classes = all_classes
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
    std::string filename = "null";
    base::Optional<Size> size;
  };

  /*--urge(name:initialize)--*/
  static scoped_refptr<Bitmap> New(ExecutionContext* execution_context,
                                   const std::string& filename,
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
                                          const std::string& extname,
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
                        const std::string& str,
                        int32_t align,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        const std::string& str,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(scoped_refptr<Rect> rect,
                        const std::string& str,
                        int32_t align,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(scoped_refptr<Rect> rect,
                        const std::string& str,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:text_size)--*/
  virtual scoped_refptr<Rect> TextSize(const std::string& str,
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
