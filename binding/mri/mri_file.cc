// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/mri_file.h"

#include "SDL3/SDL_iostream.h"
#include "components/filesystem/io_service.h"

namespace binding {

filesystem::IOService* g_io_service = nullptr;

namespace {

struct CoreFileInfo {
  SDL_IOStream* ops;
  bool closed;
};

VALUE CreateCoreFileFrom(const base::String& filename,
                         content::ExceptionState& exception_state) {
  filesystem::IOState io_state;
  SDL_IOStream* ops = g_io_service->OpenReadRaw(filename, &io_state);
  if (io_state.error_count) {
    exception_state.ThrowError(content::ExceptionCode::IO_ERROR, "%s",
                               io_state.error_message.c_str());
    return Qnil;
  }

  CoreFileInfo* info = new CoreFileInfo;
  info->ops = ops;
  info->closed = false;

  VALUE klass = rb_const_get(rb_cObject, rb_intern("CoreFile"));
  VALUE obj = rb_obj_alloc(klass);
  MriSetStructData(obj, info);

  return obj;
}

void CoreFileFreeInstance(void* ptr) {
  CoreFileInfo* info = static_cast<CoreFileInfo*>(ptr);

  if (!info->closed)
    SDL_CloseIO(info->ops);

  delete info;
}

VALUE StringForceUTF8(VALUE arg) {
  if (RB_TYPE_P(arg, RUBY_T_STRING) && ENCODING_IS_ASCII8BIT(arg))
    rb_enc_associate_index(arg, rb_utf8_encindex());

  return arg;
}

VALUE Utf8CustomProc(VALUE arg, VALUE proc) {
  VALUE obj = StringForceUTF8(arg);
  obj = rb_funcall2(proc, rb_intern("call"), 1, &obj);

  return obj;
}

}  // namespace

MRI_DEFINE_DATATYPE(CoreFile, "CoreFile", CoreFileFreeInstance);

MRI_METHOD(corefile_read) {
  CoreFileInfo* info = MriGetStructData<CoreFileInfo>(self);

  int length = -1;
  MriParseArgsTo(argc, argv, "i", &length);

  if (length == -1) {
    Sint64 cur = SDL_TellIO(info->ops);
    Sint64 end = SDL_SeekIO(info->ops, 0, SDL_IO_SEEK_END);
    length = end - cur;
    SDL_SeekIO(info->ops, cur, SDL_IO_SEEK_SET);
  }

  if (!length)
    return Qnil;

  VALUE data = rb_str_new(0, length);
  SDL_ReadIO(info->ops, RSTRING_PTR(data), length);

  return data;
}

MRI_METHOD(corefile_getbyte) {
  CoreFileInfo* info = MriGetStructData<CoreFileInfo>(self);

  unsigned char byte;
  size_t result = SDL_ReadIO(info->ops, &byte, 1);

  return (result == 1) ? rb_fix_new(byte) : Qnil;
}

MRI_METHOD(corefile_binmode) {
  return Qnil;
}

MRI_METHOD(corefile_close) {
  CoreFileInfo* info = MriGetStructData<CoreFileInfo>(self);

  if (!info->closed) {
    SDL_CloseIO(info->ops);
    info->closed = true;
  }

  return Qnil;
}

VALUE MriLoadData(const base::String& filename,
                  content::ExceptionState& exception_state) {
  rb_gc_start();

  VALUE port = CreateCoreFileFrom(filename, exception_state);
  if (exception_state.HadException())
    return Qnil;

  VALUE marshal_klass = rb_const_get(rb_cObject, rb_intern("Marshal"));

  VALUE result = rb_funcall2(marshal_klass, rb_intern("load"), 1, &port);
  rb_funcall2(port, rb_intern("close"), 0, NULL);

  return result;
}

MRI_METHOD(kernel_load_data) {
  base::String filename;
  MriParseArgsTo(argc, argv, "s", &filename);

  content::ExceptionState exception_state;

  VALUE data = MriLoadData(filename, exception_state);
  MriProcessException(exception_state);

  return data;
}

MRI_METHOD(kernel_save_data) {
  VALUE obj;
  VALUE filename;
  MriParseArgsTo(argc, argv, "oz", &obj, &filename);

  VALUE file = rb_file_open_str(filename, "wb");
  VALUE marshal_klass = rb_const_get(rb_cObject, rb_intern("Marshal"));

  VALUE v[] = {obj, file};
  rb_funcall2(marshal_klass, rb_intern("dump"), 2, v);

  rb_io_close(file);

  return Qnil;
}

MRI_METHOD(marshal_load_utf8) {
  VALUE port, proc = Qnil;

  MriParseArgsTo(argc, argv, "o|o", &port, &proc);

  VALUE utf8Proc;
  if (NIL_P(proc))
    utf8Proc = rb_proc_new(RUBY_METHOD_FUNC(StringForceUTF8), Qnil);
  else
    utf8Proc = rb_proc_new(RUBY_METHOD_FUNC(Utf8CustomProc), proc);

  VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));

  VALUE v[] = {port, utf8Proc};
  return rb_funcall2(marsh, rb_intern("load_utf8_alias_"), 2, v);
}

void InitCoreFileBinding() {
  VALUE klass = rb_define_class("CoreFile", rb_cIO);
  rb_define_alloc_func(klass, MriClassAllocate<&kCoreFileDataType>);

  MriDefineMethod(klass, "read", corefile_read);
  MriDefineMethod(klass, "getbyte", corefile_getbyte);
  MriDefineMethod(klass, "binmode", corefile_binmode);
  MriDefineMethod(klass, "close", corefile_close);

  MriDefineModuleFunction(rb_mKernel, "load_data", kernel_load_data);
  MriDefineModuleFunction(rb_mKernel, "save_data", kernel_save_data);

  VALUE marshal_klass = rb_const_get(rb_cObject, rb_intern("Marshal"));
  rb_define_alias(rb_singleton_class(marshal_klass), "load_utf8_alias_",
                  "load");
  MriDefineModuleFunction(marshal_klass, "load", marshal_load_utf8);
}

}  // namespace binding
