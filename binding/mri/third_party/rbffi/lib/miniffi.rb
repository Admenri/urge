# ===== abstract_memory.rb =====
module FFI
  class AbstractMemory
    LONG_MAX = FFI::Pointer.new(1).size
    private_constant :LONG_MAX
    def size_limit?
      size != LONG_MAX
    end
  end
end

# ===== data_converter.rb =====
module FFI
  module DataConverter
    def native_type(type = nil)
      if type
        @native_type = FFI.find_type(type)
      else
        native_type = @native_type
        unless native_type
          raise NotImplementedError, 'native_type method not overridden and no native_type set'
        end
        native_type
      end
    end
    def to_native(value, ctx)
      value
    end
    def from_native(value, ctx)
      value
    end
  end
end

# ===== autopointer.rb =====
module FFI
  class AutoPointer < Pointer
    extend DataConverter
    def initialize(ptr, proc=nil)
      raise TypeError, "Invalid pointer" if ptr.nil? || !ptr.kind_of?(Pointer) ||
          ptr.kind_of?(MemoryPointer) || ptr.kind_of?(AutoPointer)
      super(ptr.type_size, ptr)
      @releaser = if proc
                    if not proc.respond_to?(:call)
                      raise RuntimeError.new("proc must be callable")
                    end
                    Releaser.new(ptr, proc)
                  else
                    if not self.class.respond_to?(:release, true)
                      raise RuntimeError.new("no release method defined")
                    end
                    Releaser.new(ptr, self.class.method(:release))
                  end
      ObjectSpace.define_finalizer(self, @releaser)
      self
    end
    def free
      @releaser.free
    end
    def autorelease=(autorelease)
      raise FrozenError.new("can't modify frozen #{self.class}") if frozen?
      @releaser.autorelease=(autorelease)
    end
    def autorelease?
      @releaser.autorelease
    end
    class Releaser
      attr_accessor :autorelease
      def initialize(ptr, proc)
        @ptr = ptr
        @proc = proc
        @autorelease = true
      end
      def free
        if @ptr
          release(@ptr)
          @autorelease = false
          @ptr = nil
          @proc = nil
        end
      end
      def call(*args)
        release(@ptr) if @autorelease && @ptr
      end
      def release(ptr)
        @proc.call(ptr)
      end
    end
    def self.native_type
      if not self.respond_to?(:release, true)
        raise RuntimeError.new("no release method defined for #{self.inspect}")
      end
      Type::POINTER
    end
    def self.from_native(val, ctx)
      self.new(val)
    end
  end
end

# ===== compat.rb =====
module FFI
  if defined?(Ractor.make_shareable)
    def self.make_shareable(obj)
      Ractor.make_shareable(obj)
    end
  else
    def self.make_shareable(obj)
      obj.freeze
    end
  end
end

# ===== dynamic_library.rb =====
module FFI
  class DynamicLibrary
    def self.load_library(name, flags)
      FFI::DynamicLibrary.open(name == FFI::CURRENT_PROCESS ? nil : name, RTLD_LAZY | RTLD_LOCAL)
    end
    private_class_method :load_library
  end
end

# ===== enum.rb =====
module FFI
  class Enums
    def initialize
      @all_enums = Array.new
      @tagged_enums = Hash.new
      @symbol_map = Hash.new
    end
    def <<(enum)
      @all_enums << enum
      @tagged_enums[enum.tag] = enum unless enum.tag.nil?
      @symbol_map.merge!(enum.symbol_map)
    end
    def find(query)
      if @tagged_enums.has_key?(query)
        @tagged_enums[query]
      else
        @all_enums.detect { |enum| enum.symbols.include?(query) }
      end
    end
    def __map_symbol(symbol)
      @symbol_map[symbol]
    end
  end
  class Enum
    include DataConverter
    attr_reader :tag
    attr_reader :native_type
    def initialize(*args)
      @native_type = args.first.kind_of?(FFI::Type) ? args.shift : Type::INT
      info, @tag = *args
      @kv_map = Hash.new
      unless info.nil?
        last_cst = nil
        value = 0
        info.each do |i|
          case i
          when Symbol
            raise ArgumentError, "duplicate enum key" if @kv_map.has_key?(i)
            @kv_map[i] = value
            last_cst = i
            value += 1
          when Integer
            @kv_map[last_cst] = i
            value = i+1
          end
        end
      end
      @vk_map = @kv_map.invert
    end
    def symbols
      @kv_map.keys
    end
    def [](query)
      case query
      when Symbol
        @kv_map[query]
      when Integer
        @vk_map[query]
      end
    end
    alias find []
    def symbol_map
      @kv_map
    end
    alias to_h symbol_map
    alias to_hash symbol_map
    def to_native(val, ctx)
      @kv_map[val] || if val.is_a?(Integer)
        val
      elsif val.respond_to?(:to_int)
        val.to_int
      else
        raise ArgumentError, "invalid enum value, #{val.inspect}"
      end
    end
    def from_native(val, ctx)
      @vk_map[val] || val
    end
  end
  class Bitmask < Enum
    def initialize(*args)
      @native_type = args.first.kind_of?(FFI::Type) ? args.shift : Type::INT
      @signed = [Type::INT8, Type::INT16, Type::INT32, Type::INT64].include?(@native_type)
      info, @tag = *args
      @kv_map = Hash.new
      unless info.nil?
        last_cst = nil
        value = 0
        info.each do |i|
          case i
          when Symbol
            raise ArgumentError, "duplicate bitmask key" if @kv_map.has_key?(i)
            @kv_map[i] = 1 << value
            last_cst = i
            value += 1
          when Integer
            raise ArgumentError, "bitmask index should be positive" if i<0
            @kv_map[last_cst] = 1 << i
            value = i+1
          end
        end
      end
      @vk_map = @kv_map.invert
    end
    def [](*query)
      flat_query = query.flatten
      raise ArgumentError, "query should be homogeneous, #{query.inspect}" unless flat_query.all? { |o| o.is_a?(Symbol) } || flat_query.all? { |o| o.is_a?(Integer) || o.respond_to?(:to_int) }
      case flat_query[0]
      when Symbol
        flat_query.inject(0) do |val, o|
          v = @kv_map[o]
          if v then val | v else val end
        end
      when Integer, ->(o) { o.respond_to?(:to_int) }
        val = flat_query.inject(0) { |mask, o| mask |= o.to_int }
        @kv_map.select { |_, v| v & val != 0 }.keys
      end
    end
    def to_native(query, ctx)
      return 0 if query.nil?
      flat_query = [query].flatten
      res = flat_query.inject(0) do |val, o|
        case o
        when Symbol
          v = @kv_map[o]
          raise ArgumentError, "invalid bitmask value, #{o.inspect}" unless v
          val | v
        when Integer
          val | o
        when ->(obj) { obj.respond_to?(:to_int) }
          val | o.to_int
        else
          raise ArgumentError, "invalid bitmask value, #{o.inspect}"
        end
      end
      if @signed && res >= (1 << (@native_type.size * 8 - 1))
        res = -(-res & ((1 << (@native_type.size * 8)) - 1))
      end
      res
    end
    def from_native(val, ctx)
      flags = @kv_map.select { |_, v| v & val != 0 }
      list = flags.keys
      val &= (1 << (@native_type.size * 8)) - 1 if @signed
      remainder = val ^ flags.values.reduce(0, :|)
      list.push remainder unless remainder == 0
      return list
    end
  end
end

# ===== errno.rb =====
module FFI
  def self.errno
    FFI::LastError.error
  end
  def self.errno=(error)
    FFI::LastError.error = error
  end
end

# ===== ffi.rb =====
module FFI
  module ModernForkTracking
    def _fork
      pid = super
      if pid == 0
        FFI._async_cb_dispatcher_atfork_child
      end
      pid
    end
  end
  module LegacyForkTracking
    module KernelExt
      def fork
        if block_given?
          super do
            FFI._async_cb_dispatcher_atfork_child
            yield
          end
        else
          pid = super
          FFI._async_cb_dispatcher_atfork_child if pid.nil?
          pid
        end
      end
    end
    module KernelExtPrivate
      include KernelExt
      private :fork
    end
    module IOExt
      def popen(*args)
        return super unless args[0] == '-'
        super(*args) do |pipe|
          FFI._async_cb_dispatcher_atfork_child if pipe.nil?
          yield pipe
        end
      end
      ruby2_keywords :popen if respond_to?(:ruby2_keywords)
    end
  end
  if Process.respond_to?(:_fork)
    ::Process.singleton_class.prepend(ModernForkTracking)
  elsif Process.respond_to?(:fork)
    ::Object.prepend(LegacyForkTracking::KernelExtPrivate)
    ::Object.singleton_class.prepend(LegacyForkTracking::KernelExt)
    ::Kernel.prepend(LegacyForkTracking::KernelExtPrivate)
    ::Kernel.singleton_class.prepend(LegacyForkTracking::KernelExt)
    ::IO.singleton_class.prepend(LegacyForkTracking::IOExt)
  end
end

# ===== function.rb =====
module FFI
  class Function
    if private_method_defined?(:type)
      def return_type
        type.return_type
      end
      def param_types
        type.param_types
      end
    end
    module RegisterAttach
      def attach(mod, name)
        funcs = mod.instance_variable_defined?("@ffi_functions") && mod.instance_variable_get("@ffi_functions")
        unless funcs
          funcs = {}
          mod.instance_variable_set("@ffi_functions", funcs)
        end
        funcs[name.to_sym] = self
        super
      end
    end
    private_constant :RegisterAttach
    prepend RegisterAttach
  end
end

# ===== io.rb =====
module FFI
  module IO
    def self.for_fd(fd, mode = "r")
      ::IO.for_fd(fd, mode)
    end
    def self.native_read(io, buf, len)
      tmp = io.read(len)
      return -1 unless tmp
      buf.put_bytes(0, tmp)
      tmp.length
    end
  end
end

# ===== library.rb =====
module FFI
  CURRENT_PROCESS = USE_THIS_PROCESS_AS_LIBRARY = FFI.make_shareable(Object.new)
  def self.map_library_name(lib)
    lib.to_s
  end
  class NotFoundError < LoadError
    def initialize(function, *libraries)
      super("Function '#{function}' not found in [#{libraries[0].nil? ? 'current process' : libraries.join(", ")}]")
    end
  end
  module Library
    CURRENT_PROCESS = FFI::CURRENT_PROCESS
    def self.extended(mod)
      raise RuntimeError.new("must only be extended by module") unless mod.kind_of?(::Module)
    end
    def ffi_lib(*names)
      raise LoadError.new("library names list must not be empty") if names.empty?
      lib_flags = defined?(@ffi_lib_flags) && @ffi_lib_flags
      @ffi_libs = names.map do |name|
        FFI::DynamicLibrary.send(:load_library, name, lib_flags)
      end
    end
    def ffi_convention(convention = nil)
      @ffi_convention ||= :default
      @ffi_convention = convention if convention
      @ffi_convention
    end
    def ffi_libraries
      raise LoadError.new("no library specified") if !defined?(@ffi_libs) || @ffi_libs.empty?
      @ffi_libs
    end
    FlagsMap = {
      :global => DynamicLibrary::RTLD_GLOBAL,
      :local => DynamicLibrary::RTLD_LOCAL,
      :lazy => DynamicLibrary::RTLD_LAZY,
      :now => DynamicLibrary::RTLD_NOW
    }
    def ffi_lib_flags(*flags)
      @ffi_lib_flags = flags.inject(0) { |result, f| result | FlagsMap[f] }
    end
    def attach_function(name, func, args, returns = nil, options = nil)
      mname, a2, a3, a4, a5 = name, func, args, returns, options
      cname, arg_types, ret_type, opts = (a4 && (a2.is_a?(String) || a2.is_a?(Symbol))) ? [ a2, a3, a4, a5 ] : [ mname.to_s, a2, a3, a4 ]
      arg_types = arg_types.map { |e| find_type(e) }
      options = {
        :convention => ffi_convention,
        :type_map => defined?(@ffi_typedefs) ? @ffi_typedefs : nil,
        :blocking => defined?(@blocking) && @blocking,
        :enums => defined?(@ffi_enums) ? @ffi_enums : nil,
      }
      @blocking = false
      options.merge!(opts) if opts && opts.is_a?(Hash)
      invokers = []
      ffi_libraries.each do |lib|
        if invokers.empty?
          begin
            function = nil
            function_names(cname, arg_types).find do |fname|
              function = lib.find_function(fname)
            end
            raise LoadError unless function
            invokers << if arg_types[-1] == FFI::NativeType::VARARGS
              VariadicInvoker.new(function, arg_types, find_type(ret_type), options)
            else
              Function.new(find_type(ret_type), arg_types, function, options)
            end
          rescue LoadError
          end
        end
      end
      invoker = invokers.compact.shift
      raise FFI::NotFoundError.new(cname.to_s, ffi_libraries.map { |lib| lib.name }) unless invoker
      invoker.attach(self, mname.to_s)
      invoker
    end
    def function_names(name, arg_types)
      result = [name.to_s]
      if ffi_convention == :stdcall
        size = arg_types.inject(0) do |mem, arg|
          size = arg.size
          size += (4 - size) % 4
          mem + size
        end
        result << "_#{name.to_s}@#{size}"
        result << "#{name.to_s}@#{size}"
      end
      result
    end
    def attach_variable(mname, a1, a2 = nil)
      cname, type = a2 ? [ a1, a2 ] : [ mname.to_s, a1 ]
      mname = mname.to_sym
      address = nil
      ffi_libraries.each do |lib|
        begin
          address = lib.find_variable(cname.to_s)
          break unless address.nil?
        rescue LoadError
        end
      end
      raise FFI::NotFoundError.new(cname, ffi_libraries) if address.nil? || address.null?
      if type.is_a?(Class) && type < FFI::Struct
        s = s = type.new(address)
        self.module_eval <<-code, __FILE__, __LINE__
          @ffi_gsvars = {} unless defined?(@ffi_gsvars)
          @ffi_gsvars[
          def self.
            @ffi_gsvars[
          end
        code
      else
        sc = Class.new(FFI::Struct)
        sc.layout :gvar, find_type(type)
        s = sc.new(address)
        self.module_eval <<-code, __FILE__, __LINE__
          @ffi_gvars = {} unless defined?(@ffi_gvars)
          @ffi_gvars[
          def self.
            @ffi_gvars[
          end
          def self.
            @ffi_gvars[
          end
        code
      end
      address
    end
    def callback(*args)
      raise ArgumentError, "wrong number of arguments" if args.length < 2 || args.length > 3
      name, params, ret = if args.length == 3
        args
      else
        [ nil, args[0], args[1] ]
      end
      native_params = params.map { |e| find_type(e) }
      raise ArgumentError, "callbacks cannot have variadic parameters" if native_params.include?(FFI::Type::VARARGS)
      options = Hash.new
      options[:convention] = ffi_convention
      options[:enums] = @ffi_enums if defined?(@ffi_enums)
      ret_type = find_type(ret)
      if ret_type == Type::STRING
        raise TypeError, ":string is not allowed as return type of callbacks"
      end
      cb = FFI::CallbackInfo.new(ret_type, native_params, options)
      unless name.nil?
        typedef cb, name
      end
      cb
    end
    def typedef(old, add, info=nil)
      @ffi_typedefs = Hash.new unless defined?(@ffi_typedefs)
      @ffi_typedefs[add] = if old.kind_of?(FFI::Type)
        old
      elsif @ffi_typedefs.has_key?(old)
        @ffi_typedefs[old]
      elsif old.is_a?(DataConverter)
        FFI::Type::Mapped.new(old)
      elsif old == :enum
        if add.kind_of?(Array)
          self.enum(add)
        else
          self.enum(info, add)
        end
      else
        FFI.find_type(old)
      end
    end
    def find_type(t)
      if t.kind_of?(Type)
        t
      elsif defined?(@ffi_typedefs) && @ffi_typedefs.has_key?(t)
        @ffi_typedefs[t]
      elsif t.is_a?(Class) && t < Struct
        Type::POINTER
      elsif t.is_a?(DataConverter)
        typedef Type::Mapped.new(t), t
      end || FFI.find_type(t)
    end
    private
    def generic_enum(klass, *args)
      native_type = args.first.kind_of?(FFI::Type) ? args.shift : nil
      name, values = if args[0].kind_of?(Symbol) && args[1].kind_of?(Array)
        [ args[0], args[1] ]
      elsif args[0].kind_of?(Array)
        [ nil, args[0] ]
      else
        [ nil, args ]
      end
      @ffi_enums = FFI::Enums.new unless defined?(@ffi_enums)
      @ffi_enums << (e = native_type ? klass.new(native_type, values, name) : klass.new(values, name))
      typedef(e, name) if name
      e
    end
    public
    def enum(*args)
      generic_enum(FFI::Enum, *args)
    end
    def bitmask(*args)
      generic_enum(FFI::Bitmask, *args)
    end
    def enum_type(name)
      @ffi_enums.find(name) if defined?(@ffi_enums)
    end
    def enum_value(symbol)
      @ffi_enums.__map_symbol(symbol)
    end
    def attached_functions
      @ffi_functions || {}
    end
    def attached_variables
      (
        (defined?(@ffi_gsvars) ? @ffi_gsvars : {}).map do |name, gvar|
          [name, gvar.class]
        end +
        (defined?(@ffi_gvars) ? @ffi_gvars : {}).map do |name, gvar|
          [name, gvar.layout[:gvar].type]
        end
      ).to_h
    end
    def freeze
      instance_variables.each do |name|
        var = instance_variable_get(name)
        FFI.make_shareable(var)
      end
      nil
    end
  end
end

# ===== managedstruct.rb =====
module FFI
  class ManagedStruct < FFI::Struct
    def initialize(pointer=nil)
      raise NoMethodError, "release() not implemented for class #{self}" unless self.class.respond_to?(:release, true)
      raise ArgumentError, "Must supply a pointer to memory for the Struct" unless pointer
      super AutoPointer.new(pointer, self.class.method(:release))
    end
  end
end

# ===== pointer.rb =====
module FFI
  class Pointer
    def self.size
      SIZE
    end unless respond_to?(:size)
    def read_string(len=nil)
      if len
        return ''.b if len == 0
        get_bytes(0, len)
      else
        get_string(0)
      end
    end unless method_defined?(:read_string)
    def read_string_length(len)
      get_bytes(0, len)
    end unless method_defined?(:read_string_length)
    def read_string_to_null
      get_string(0)
    end unless method_defined?(:read_string_to_null)
    def write_string_length(str, len)
      put_bytes(0, str, 0, len)
    end unless method_defined?(:write_string_length)
    def write_string(str, len=nil)
      len = str.bytesize unless len
      put_bytes(0, str, 0, len)
    end unless method_defined?(:write_string)
    def read_array_of_type(type, reader, length)
      ary = []
      size = FFI.type_size(type)
      tmp = self
      length.times { |j|
        ary << tmp.send(reader)
        tmp += size unless j == length-1
      }
      ary
    end unless method_defined?(:read_array_of_type)
    def write_array_of_type(type, writer, ary)
      size = FFI.type_size(type)
      ary.each_with_index { |val, i|
        break unless i < self.size
        self.send(writer, i * size, val)
      }
      self
    end unless method_defined?(:write_array_of_type)
    def to_ptr
      self
    end unless method_defined?(:to_ptr)
    def read(type)
      get(type, 0)
    end unless method_defined?(:read)
    def write(type, value)
      put(type, 0, value)
    end unless method_defined?(:write)
  end
end

# ===== struct_by_reference.rb =====
module FFI
  class StructByReference
    include DataConverter
    attr_reader :struct_class
    def initialize(struct_class)
      unless Class === struct_class and struct_class < FFI::Struct
        raise TypeError, 'wrong type (expected subclass of FFI::Struct)'
      end
      @struct_class = struct_class
    end
    def native_type(_type = nil)
      FFI::Type::POINTER
    end
    def to_native(value, ctx)
      return Pointer::NULL if value.nil?
      unless @struct_class === value
        raise TypeError, "wrong argument type #{value.class} (expected #{@struct_class})"
      end
      value.pointer
    end
    def from_native(value, ctx)
      @struct_class.new(value)
    end
  end
end

# ===== struct_layout.rb =====
module FFI
  class StructLayout
    def offsets
      members.map { |m| [ m, self[m].offset ] }
    end
    def offset_of(field_name)
      self[field_name].offset
    end
    class Enum < Field
      def get(ptr)
        type.find(ptr.get_int(offset))
      end
      def put(ptr, value)
        ptr.put_int(offset, type.find(value))
      end
    end
    class InnerStruct < Field
      def get(ptr)
        type.struct_class.new(ptr.slice(self.offset, self.size))
      end
     def put(ptr, value)
       raise TypeError, "wrong value type (expected #{type.struct_class})" unless value.is_a?(type.struct_class)
       ptr.slice(self.offset, self.size).__copy_from__(value.pointer, self.size)
     end
    end
    class Mapped < Field
      def initialize(name, offset, type, orig_field)
        @orig_field = orig_field
        super(name, offset, type)
      end
      def get(ptr)
        type.from_native(@orig_field.get(ptr), nil)
      end
      def put(ptr, value)
        @orig_field.put(ptr, type.to_native(value, nil))
      end
    end
  end
end

# ===== struct_layout_builder.rb =====
module FFI
  class StructLayoutBuilder
    attr_reader :size
    attr_reader :alignment
    def initialize
      @size = 0
      @alignment = 1
      @min_alignment = 1
      @packed = false
      @union = false
      @fields = Array.new
    end
    def size=(size)
      @size = size if size > @size
    end
    def alignment=(align)
      @alignment = align if align > @alignment
      @min_alignment = align
    end
    def union=(is_union)
      @union = is_union
    end
    def union?
      @union
    end
    def packed=(packed)
      if packed.is_a?(0.class)
        @alignment = packed
        @packed = packed
      else
        @packed = packed ? 1 : 0
      end
    end
    NUMBER_TYPES = [
      Type::INT8,
      Type::UINT8,
      Type::INT16,
      Type::UINT16,
      Type::INT32,
      Type::UINT32,
      Type::LONG,
      Type::ULONG,
      Type::INT64,
      Type::UINT64,
      Type::FLOAT32,
      Type::FLOAT64,
      Type::LONGDOUBLE,
      Type::BOOL,
    ].freeze
    def add(name, type, offset = nil)
      if offset.nil? || offset == -1
        offset = @union ? 0 : align(@size, @packed ? [ @packed, type.alignment ].min : [ @min_alignment, type.alignment ].max)
      end
      field = type.is_a?(StructLayout::Field) ? type : field_for_type(name, offset, type)
      @fields << field
      @alignment = [ @alignment, field.alignment ].max unless @packed
      @size = [ @size, field.size + (@union ? 0 : field.offset) ].max
      return self
    end
    def add_field(name, type, offset = nil)
      add(name, type, offset)
    end
    def add_struct(name, type, offset = nil)
      add(name, Type::Struct.new(type), offset)
    end
    def add_array(name, type, count, offset = nil)
      add(name, Type::Array.new(type, count), offset)
    end
    def build
      size = @packed ? @size : align(@size, @alignment)
      layout = StructLayout.new(@fields, size, @alignment)
      layout.__union! if @union
      layout
    end
    private
    def align(offset, align)
      align + ((offset - 1) & ~(align - 1));
    end
    def field_for_type(name, offset, type)
      field_class = case
      when type.is_a?(Type::Function)
        StructLayout::Function
      when type.is_a?(Type::Struct)
        StructLayout::InnerStruct
      when type.is_a?(Type::Array)
        StructLayout::Array
      when type.is_a?(FFI::Enum)
        StructLayout::Enum
      when NUMBER_TYPES.include?(type)
        StructLayout::Number
      when type == Type::POINTER
        StructLayout::Pointer
      when type == Type::STRING
        StructLayout::String
      when type.is_a?(Class) && type < StructLayout::Field
        type
      when type.is_a?(DataConverter)
        return StructLayout::Mapped.new(name, offset, Type::Mapped.new(type), field_for_type(name, offset, type.native_type))
      when type.is_a?(Type::Mapped)
        return StructLayout::Mapped.new(name, offset, type, field_for_type(name, offset, type.native_type))
      else
        raise TypeError, "invalid struct field type #{type.inspect}"
      end
      field_class.new(name, offset, type)
    end
  end
end

# ===== struct.rb =====
module FFI
  class Struct
    def size
      self.class.size
    end
    def alignment
      self.class.alignment
    end
    alias_method :align, :alignment
    def offset_of(name)
      self.class.offset_of(name)
    end
    def members
      self.class.members
    end
    def values
      members.map { |m| self[m] }
    end
    def offsets
      self.class.offsets
    end
    def clear
      pointer.clear
      self
    end
    def to_ptr
      pointer
    end
    def self.size
      defined?(@layout) ? @layout.size : defined?(@size) ? @size : 0
    end
    def self.size=(size)
      raise ArgumentError, "Size already set" if defined?(@size) || defined?(@layout)
      @size = size
    end
    def self.alignment
      @layout.alignment
    end
    def self.members
      @layout.members
    end
    def self.offsets
      @layout.offsets
    end
    def self.offset_of(name)
      @layout.offset_of(name)
    end
    def self.in
      ptr(:in)
    end
    def self.out
      ptr(:out)
    end
    def self.ptr(flags = :inout)
      @ref_data_type ||= Type::Mapped.new(StructByReference.new(self))
    end
    def self.val
      @val_data_type ||= StructByValue.new(self)
    end
    def self.by_value
      self.val
    end
    def self.by_ref(flags = :inout)
      self.ptr(flags)
    end
    class ManagedStructConverter < StructByReference
      def initialize(struct_class)
        super(struct_class)
        raise NoMethodError, "release() not implemented for class #{struct_class}" unless struct_class.respond_to? :release
        @method = struct_class.method(:release)
      end
      def from_native(ptr, ctx)
        struct_class.new(AutoPointer.new(ptr, @method))
      end
    end
    def self.auto_ptr
      @managed_type ||= Type::Mapped.new(ManagedStructConverter.new(self))
    end
    class << self
      public
      def layout(*spec)
        return @layout if spec.size == 0
        warn "[DEPRECATION] Struct layout is already defined for class #{self.inspect}. Redefinition as in #{caller[0]} will be disallowed in ffi-2.0." if defined?(@layout)
        builder = StructLayoutBuilder.new
        builder.union = self < Union
        builder.packed = @packed if defined?(@packed)
        builder.alignment = @min_alignment if defined?(@min_alignment)
        if spec[0].kind_of?(Hash)
          hash_layout(builder, spec)
        else
          array_layout(builder, spec)
        end
        builder.size = @size if defined?(@size) && @size > builder.size
        cspec = builder.build
        @layout = cspec unless self == Struct
        @size = cspec.size
        return cspec
      end
      protected
      def callback(params, ret)
        mod = enclosing_module
        ret_type = find_type(ret, mod)
        if ret_type == Type::STRING
          raise TypeError, ":string is not allowed as return type of callbacks"
        end
        FFI::CallbackInfo.new(ret_type, params.map { |e| find_type(e, mod) })
      end
      def packed(packed = 1)
        @packed = packed
      end
      alias :pack :packed
      def aligned(alignment = 1)
        @min_alignment = alignment
      end
      alias :align :aligned
      def enclosing_module
        begin
          mod = self.name.split("::")[0..-2].inject(Object) { |obj, c| obj.const_get(c) }
          if mod.respond_to?(:find_type) && (mod.is_a?(FFI::Library) || mod < FFI::Struct)
            mod
          end
        rescue Exception
          nil
        end
      end
      def find_field_type(type, mod = enclosing_module)
        if type.kind_of?(Class) && type < Struct
          FFI::Type::Struct.new(type)
        elsif type.kind_of?(Class) && type < FFI::StructLayout::Field
          type
        elsif type.kind_of?(::Array)
          FFI::Type::Array.new(find_field_type(type[0]), type[1])
        else
          find_type(type, mod)
        end
      end
      def find_type(type, mod = enclosing_module)
        if mod
          mod.find_type(type)
        end || FFI.find_type(type)
      end
      private
      def hash_layout(builder, spec)
        spec[0].each do |name, type|
          builder.add name, find_field_type(type), nil
        end
      end
      def array_layout(builder, spec)
        i = 0
        while i < spec.size
          name, type = spec[i, 2]
          i += 2
          if spec[i].kind_of?(Integer)
            offset = spec[i]
            i += 1
          else
            offset = nil
          end
          builder.add name, find_field_type(type), offset
        end
      end
    end
  end
end

# ===== types.rb =====
module FFI
  unless defined?(custom_typedefs)
    def self.custom_typedefs
      TypeDefs
    end
    writable_typemap = true
  end
  class << self
    private :custom_typedefs
    def typedef(old, add)
      tm = custom_typedefs
      tm[add] = find_type(old)
    end
    def add_typedef(old, add)
      typedef old, add
    end
    private def __typedef(old, add)
      TypeDefs[add] = find_type(old, TypeDefs)
    end
    def find_type(name, type_map = nil)
      if name.is_a?(Type)
        name
      elsif type_map&.has_key?(name)
        type_map[name]
      elsif (tm=custom_typedefs).has_key?(name)
        tm[name]
      elsif TypeDefs.has_key?(name)
        TypeDefs[name]
      elsif name.is_a?(DataConverter)
        tm = (type_map || custom_typedefs)
        tm[name] = Type::Mapped.new(name)
      else
        raise TypeError, "unable to resolve type '#{name}'"
      end
    end
    def type_size(type)
      find_type(type).size
    end
  end
  TypeDefs.merge!({
      :void => Type::VOID,
      :bool => Type::BOOL,
      :string => Type::STRING,
      :char => Type::CHAR,
      :uchar => Type::UCHAR,
      :short => Type::SHORT,
      :ushort => Type::USHORT,
      :int => Type::INT,
      :uint => Type::UINT,
      :long => Type::LONG,
      :ulong => Type::ULONG,
      :long_long => Type::LONG_LONG,
      :ulong_long => Type::ULONG_LONG,
      :float => Type::FLOAT,
      :double => Type::DOUBLE,
      :long_double => Type::LONGDOUBLE,
      :pointer => Type::POINTER,
      :int8 => Type::INT8,
      :uint8 => Type::UINT8,
      :int16 => Type::INT16,
      :uint16 => Type::UINT16,
      :int32 => Type::INT32,
      :uint32 => Type::UINT32,
      :int64 => Type::INT64,
      :uint64 => Type::UINT64,
      :buffer_in => Type::BUFFER_IN,
      :buffer_out => Type::BUFFER_OUT,
      :buffer_inout => Type::BUFFER_INOUT,
      :varargs => Type::VARARGS,
  })
  class StrPtrConverter
    extend DataConverter
    native_type Type::POINTER
    def self.from_native(val, ctx)
      [ val.null? ? nil : val.get_string(0), val ]
    end
  end
  __typedef(StrPtrConverter, :strptr)
  FFI.make_shareable(TypeDefs) unless writable_typemap
end

# ===== union.rb =====
module FFI
  class Union < FFI::Struct
    def self.builder
      b = StructLayoutBuilder.new
      b.union = true
      b
    end
  end
end

# ===== variadic.rb =====
module FFI
  class VariadicInvoker
    def call(*args, &block)
      param_types = Array.new(@fixed)
      param_values = Array.new
      @fixed.each_with_index do |t, i|
        param_values << args[i]
      end
      i = @fixed.length
      while i < args.length
        param_types << FFI.find_type(args[i], @type_map)
        param_values << args[i + 1]
        i += 2
      end
      invoke(param_types, param_values, &block)
    end
    def attach(mod, mname)
      invoker = self
      params = "*args"
      call = "call"
      mname = mname.to_sym
      mod.module_eval <<-code, __FILE__, __LINE__
        @ffi_functions = {} unless defined?(@ffi_functions)
        @ffi_functions[
        def self.
          @ffi_functions[
        end
        define_method(
      code
      invoker
    end
    def param_types
      [*@fixed, Type::Builtin::VARARGS]
    end
  end
end

# ===== version.rb =====
module FFI
  VERSION = '1.17.2'
end

