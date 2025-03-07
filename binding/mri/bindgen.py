import binding.build.aas_parser as aas
import re


class MriBindGen:
  def __init__(self):
    self.class_data = {}
    self.file_name = ""

  def setup(self, klass_data: dict):
    self.class_data = klass_data
    kname = self.class_data["class_name"]
    self.file_name = "autogen_{}_binding".format(kname.lower())

  def gen_source_declaration(self):
    template = self.class_data
    kname = template["class_name"]
    is_module = template["is_module"]

    func_body = "void Init{}Binding() {{\n".format(kname)

    if is_module:
      func_body += "VALUE klass = rb_define_module(\"{}\");\n".format(kname)
    else:
      func_body += "VALUE klass = rb_define_class(\"{}\", rb_cObject);\n".format(kname)
      func_body += "rb_define_alloc_func(klass, MriClassAllocate<&k{}DataType>);\n".format(kname)

    func_body += "\n"

    attrs = template["attributes"]
    for attr in attrs:
      aname = attr["name"]
      fname = attr["func"]
      is_static = attr["is_static"]

      preprocessor = "MRI_DECLARE_ATTRIBUTE"
      if is_module:
        preprocessor = "MRI_DECLARE_MODULE_ATTRIBUTE"
      else:
        if is_static:
          preprocessor = "MRI_DECLARE_CLASS_ATTRIBUTE"

      func_body += "{}(klass, \"{}\", {}, {});\n".format(preprocessor, aname, kname, fname)

    func_body += "\n"

    methods = template["methods"]
    defined_methods = []
    for method in methods:
      mname = method["name"]
      fname = method["func"]
      is_static = method["is_static"]

      if defined_methods.count(mname) > 0:
        continue
      defined_methods.append(mname)

      preprocessor = "MriDefineMethod"
      if is_module:
        preprocessor = "MriDefineModuleFunction"
      else:
        if is_static:
          preprocessor = "MriDefineClassMethod"

      func_body += "{}(klass, \"{}\", {}_{});\n".format(preprocessor, mname, kname, fname)

    func_body += "}\n"
    return func_body

  def gen_source_defination(self):
    template = self.class_data
    kname = template["class_name"]
    is_module = template["is_module"]

    func_body = ""
    if not is_module:
      func_body += "MRI_DEFINE_DATATYPE_REF({}, \"{}\", content::{});\n".format(kname, kname, kname)

    func_body += "\n"

    attrs = template["attributes"]
    for attr in attrs:
      fname = attr["func"]
      attr_type = attr["type"]
      is_static = attr["is_static"]

      preprocessor_prefix = ""
      if is_static:
        preprocessor_prefix = "STATIC_"

      preprocessor_suffix = ""
      if attr_type.startswith("scoped_refptr"):
        preprocessor_suffix = "OBJ"
      elif attr_type.startswith("float"):
        preprocessor_suffix = "FLOAT"
      elif attr_type.startswith("bool"):
        preprocessor_suffix = "BOOLEAN"
      else:
        preprocessor_suffix = "INTEGER"

      func_body += "MRI_DEFINE_{}ATTRIBUTE_{}({}, {});\n".format(preprocessor_prefix, preprocessor_suffix, kname, fname)

    func_body += "\n"

    methods = template["methods"]
    defined_methods = []
    for method in methods:
      defined_methods.append(method["func"])

    generated_methods = []
    for method in methods:
      func_name = method["func"]
      static_method = method["is_static"]
      if generated_methods.count(func_name) > 0:
        continue
      generated_methods.append(func_name)

      params = method["parameters"]
      func_body += "MRI_METHOD({}_{}) {{\n".format(kname, func_name)

      func_body += "switch (argc) {\n"
      for i in range(len(params)):
        func_body += "case {}: {{\n".format(len(params[i]))

        parse_template = ""
        parser_arguments = ""
        object_convertion = ""
        call_parameters = ""

        current_overload = params[i]
        for param in current_overload:
          a_type = param["type"]
          a_name = param["name"]
          is_optional = param["optional"]
          def_value = param["default_value"]

          if is_optional:
            parse_template += "|"

          if a_type.startswith("const std::string"):
            parse_template += "s"
            a_type = "std::string"
            call_parameters += a_name
          elif a_type.startswith("float"):
            parse_template += "f"
            a_type = "double"
            call_parameters += a_name
          elif a_type.startswith("bool"):
            parse_template += "b"
            call_parameters += a_name
          elif a_type.startswith("scoped_refptr"):
            parse_template += "o"
            call_parameters += a_name + "_obj"

            match = re.search(r'scoped_refptr<(\w+)>', a_type)
            if match:
              refptr_type = match.group(1)
            object_convertion += "{} {}_obj = MriCheckStructData<content::{}>({}, k{}DataType);\n".format(a_type, a_name, refptr_type, a_name, refptr_type)
          else:
            parse_template += "i"
            call_parameters += a_name

          func_body += "{} {}".format(a_type, a_name)
          if is_optional:
            func_body += " = {}".format(def_value)
          func_body += ";\n"

          parser_arguments += "&{}".format(a_name)
          if param != current_overload[-1]:
            parser_arguments += ", "
            call_parameters += ", "

        if parse_template != "":
          func_body += "MriParseArgsTo(argc, argv, \"{}\", {});\n".format(parse_template, parser_arguments)
          func_body += object_convertion

        return_type = method["return_type"]

        return_refptr_type = ""
        match = re.search(r'scoped_refptr<(\w+)>', return_type)
        if match:
          return_refptr_type = match.group(1)

        caller_prefix = ""
        if return_type != "void":
          caller_prefix += return_type + " result_value = "

        if static_method:
          func_body += caller_prefix
          func_body += "content::{}::{}({});\n".format(kname, func_name, call_parameters)
        else:
          if not is_module:
            func_body += "scoped_refptr self_obj = MriGetStructData<{}>(self);\n".format(kname)
          else:
            func_body += "scoped_refptr self_obj = MriGetGlobalModules()->{};\n".format(kname)

          func_body += caller_prefix
          func_body += "self_obj->{}({});\n".format(func_name, call_parameters)

        if return_type != "void":
          if return_type.startswith("scoped_refptr"):
            func_body += "return MriWrapObject(result_value, k{}DataType);\n".format(return_refptr_type)
          elif return_type.startswith("float"):
            func_body += "return rb_float_new(result_value);\n"
          elif return_type.startswith("bool"):
            func_body += "return result_value ? Qtrue : Qfalse;\n"
          elif return_type.startswith("std::string"):
            func_body += "return rb_utf8_str_new(result_value.c_str(), result_value.size());\n"
          else:
            func_body += "return rb_fix_new(result_value);\n"

        func_body += "}\n"
      func_body += "}\n"
      func_body += "}\n"

    return func_body

  def gen_source(self):
    source_body = "\n"
    source_body += "#include \"binding/mri/{}.h\"\n\n".format(self.file_name)

    for dep in self.class_data["dependency"]:
      if dep != self.class_data["class_name"]:
        source_body += "#include \"binding/mri/autogen_{}_binding.h\"\n".format(dep.lower())

    source_body += "\nnamespace binding {\n"

    source_body += self.gen_source_defination()
    source_body += "\n"
    source_body += self.gen_source_declaration()

    source_body += "} // namespace binding\n"

    return source_body

  def gen_header(self):
    template = self.class_data
    kname = template["class_name"]
    is_module = template["is_module"]

    header_body = "#ifndef BINDING_MRI_AUTOGEN_{}_BINDING_H_\n".format(kname.upper())
    header_body += "#define BINDING_MRI_AUTOGEN_{}_BINDING_H_\n\n".format(kname.upper())
    header_body += "#include \"binding/mri/mri_util.h\"\n\n"
    header_body += "namespace binding {\n"

    if not is_module:
      header_body += "MRI_DECLARE_DATATYPE({});\n\n".format(kname)

    header_body += "void Init{}Binding();\n".format(kname)

    header_body += "} // namespace binding\n\n"
    header_body += "#endif // !BINDING_MRI_AUTOGEN_{}_BINDING_H_\n".format(kname.upper())

    return header_body

if __name__ == "__main__":
  cpp_code = """
      /*--urge(name:Graphics,is_module)--*/
      class URGE_RUNTIME_API Graphics : public base::RefCounted<Graphics> {
       public:
        virtual ~Graphics() = default;

        /*--urge(name:initialize)--*/
        static scoped_refptr<Graphics> New(ExecutionContext* execution_context,
                                         const std::string& filename,
                                         ExceptionState& exception_state);

        /*--urge(name:initialize)--*/
        static scoped_refptr<Graphics> New(ExecutionContext* execution_context,
                                         int32_t width,
                                         int32_t height,
                                         ExceptionState& exception_state);

        /*--urge(name:dispose)--*/
        virtual void Dispose(ExceptionState& exception_state) = 0;

        /*--urge(name:test)--*/
        virtual scoped_refptr<Bitmap> Test(ExceptionState& exception_state) = 0;

        /*--urge(name:set,optional:value=nullptr)--*/
        virtual void Set(scoped_refptr<Font> value, ExceptionState& exception_state) = 0;

        /*--urge(name:set,optional:value=0,optional:opacity=255)--*/
        virtual void Set(int32_t value, int32_t opacity, ExceptionState& exception_state) = 0;

        /*--urge(name:font)--*/
        URGE_EXPORT_ATTRIBUTE(Font, scoped_refptr<Font>);

        /*--urge(name:static_font)--*/
        URGE_EXPORT_STATIC_ATTRIBUTE(StaticFont, scoped_refptr<Font>);
      };

      /*--urge(name:Bitmap)--*/
      class URGE_RUNTIME_API Bitmap : public base::RefCounted<Bitmap> {
       public:
        virtual ~Bitmap() = default;

        /*--urge(name:initialize)--*/
        static scoped_refptr<Bitmap> New(ExecutionContext* execution_context,
                                       const std::string& filename,
                                       ExceptionState& exception_state);

        /*--urge(name:dispose)--*/
        virtual void Dispose(ExceptionState& exception_state) = 0;

        /*--urge(name:test)--*/
        virtual scoped_refptr<Bitmap> Test(ExceptionState& exception_state) = 0;

        /*--urge(name:set,optional:value=0,optional:opacity=255)--*/
        virtual void Set(int32_t value, ExceptionState& exception_state) = 0;

        /*--urge(name:set,optional:value=0,optional:opacity=255)--*/
        virtual void Set(int32_t value, int32_t opacity, ExceptionState& exception_state) = 0;

        /*--urge(name:font)--*/
        URGE_EXPORT_ATTRIBUTE(Font, scoped_refptr<Font>);

        /*--urge(name:static_font)--*/
        URGE_EXPORT_STATIC_ATTRIBUTE(StaticFont, scoped_refptr<Font>);
      };
      """

  parser = aas.APIParser()
  parser.parse(cpp_code)

  for item in parser.classes:
    gen = MriBindGen()
    gen.setup(item)
    print("====================header=====================")
    print(gen.gen_header())
    print("=====================body=====================")
    print(gen.gen_source())
    print("=====================end=======================")
