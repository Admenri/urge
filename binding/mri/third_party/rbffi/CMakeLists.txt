cmake_minimum_required(VERSION 3.20.1)

add_library(rb_ffi STATIC)
target_link_libraries(rb_ffi PUBLIC mri_ruby ffi)
target_include_directories(rb_ffi PUBLIC ext)
target_sources(rb_ffi PRIVATE
  ext/AbstractMemory.c
  ext/AbstractMemory.h
  ext/ArrayType.c
  ext/ArrayType.h
  ext/Buffer.c
  ext/Call.c
  ext/Call.h
  ext/ClosurePool.c
  ext/ClosurePool.h
  ext/compat.h
  ext/DynamicLibrary.c
  ext/DynamicLibrary.h
  ext/ffi.c
  ext/Function.c
  ext/Function.h
  ext/FunctionInfo.c
  ext/LastError.c
  ext/LastError.h
  ext/LongDouble.c
  ext/LongDouble.h
  ext/MappedType.c
  ext/MappedType.h
  ext/MemoryPointer.c
  ext/MemoryPointer.h
  ext/MethodHandle.c
  ext/MethodHandle.h
  ext/Platform.c
  ext/Platform.h
  ext/Pointer.c
  ext/Pointer.h
  ext/rbffi_endian.h
  ext/rbffi.h
  ext/Struct.c
  ext/Struct.h
  ext/StructByValue.c
  ext/StructByValue.h
  ext/StructLayout.c
  ext/Thread.c
  ext/Thread.h
  ext/Type.c
  ext/Type.h
  ext/Types.c
  ext/Types.h
  ext/Variadic.c
)
