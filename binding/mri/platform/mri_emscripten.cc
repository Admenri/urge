// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/platform/mri_emscripten.h"

#include "binding/mri/mri_util.h"

#if defined(OS_EMSCRIPTEN)
#include <emscripten.h>

EM_JS(void, emjs_storage_save_file, (const char* filePath), {
  const asyncLoader = (wakeUp) => {
    const u8Path = UTF8ToString(filePath);
    const fileData = FS.readFile(u8Path);

    const catchBlock = (err) => {
      console.log('urge.storage_save_file error', err);
    };

    window.localforage.setItem(u8Path, fileData).then(wakeUp).catch(catchBlock);
  };

  Asyncify.handleSleep(asyncLoader);
});

EM_JS(void, emjs_storage_load_file, (const char* filePath), {
  const asyncLoader = (wakeUp) => {
    const u8Path = UTF8ToString(filePath);
    const fileLoader = (data) => {
      FS.writeFile(u8Path, data);
      wakeUp();
    };

    const catchBlock = (err) => {
      console.log('urge.storage_load_file error', err);
    };

    window.localforage.getItem(u8Path).then(fileLoader).catch(catchBlock);
  };

  Asyncify.handleSleep(asyncLoader);
});

EM_JS(void, emjs_storage_load_all_files, (void), {
  const asyncLoader = (wakeUp) => {
    const iterFile = (value, key, iterationNumber) => {
      FS.writeFile(key, value);
    };

    const catchBlock = (err) => {
      console.log('urge.storage_load_all_files error', err);
    };

    window.localforage.iterate(iterFile).then(wakeUp).catch(catchBlock);
  };

  Asyncify.handleSleep(asyncLoader);
});

EM_JS(void, emjs_storage_clear, (const char* filePath), {
  const asyncLoader = (wakeUp) => {
    const u8Path = UTF8ToString(filePath);

    const catchBlock = (err) => {
      console.log('urge.storage_clear error', err);
    };

    if (u8Path == '') {
      // Clear all
      window.localforage.clear().then(wakeUp).catch(catchBlock);
    } else {
      // Clear specific target
      window.localforage.removeItem(u8Path).then(wakeUp).catch(catchBlock);
    }
  };

  Asyncify.handleSleep(asyncLoader);
});

// components/filesystem/io_service.cc
extern "C" {
void emjs_load_file_async(const char* filePath);
bool emjs_is_file_cached(const char* filePath);
}

namespace binding {

MRI_METHOD(em_execute_javascript) {
  MriCheckArgc(argc, 1);

  auto* eval_str = RSTRING_PTR(argv[0]);
  auto* result_str = emscripten_run_script_string(eval_str);
  return rb_utf8_str_new_cstr(result_str);
}

MRI_METHOD(em_storage_save_file) {
  MriCheckArgc(argc, 1);

  auto* path_str = RSTRING_PTR(argv[0]);
  emjs_storage_save_file(path_str);

  return Qnil;
}

MRI_METHOD(em_storage_load_file) {
  MriCheckArgc(argc, 1);

  auto* path_str = RSTRING_PTR(argv[0]);
  emjs_storage_load_file(path_str);

  return Qnil;
}

MRI_METHOD(em_storage_load_all_files) {
  MriCheckArgc(argc, 0);

  emjs_storage_load_all_files();

  return Qnil;
}

MRI_METHOD(em_storage_clear) {
  MriCheckArgc(argc, 1);

  auto* path_str = RSTRING_PTR(argv[0]);
  emjs_storage_clear(path_str);

  return Qnil;
}

MRI_METHOD(em_preload_file) {
  MriCheckArgc(argc, 1);

  auto* path_str = RSTRING_PTR(argv[0]);
  emjs_load_file_async(path_str);

  return Qnil;
}

MRI_METHOD(em_is_preload_file_cached) {
  MriCheckArgc(argc, 1);

  auto* path_str = RSTRING_PTR(argv[0]);
  return emjs_is_file_cached(path_str) ? Qtrue : Qfalse;
}

void InitEmscriptenBinding() {
  VALUE module = rb_define_module("Emscripten");

  MriDefineModuleFunction(module, "evaluate", em_execute_javascript);

  MriDefineModuleFunction(module, "storage_save_file", em_storage_save_file);
  MriDefineModuleFunction(module, "storage_load_file", em_storage_load_file);
  MriDefineModuleFunction(module, "storage_load_all_files",
                          em_storage_load_all_files);
  MriDefineModuleFunction(module, "storage_clear", em_storage_clear);

  MriDefineModuleFunction(module, "preload_file", em_preload_file);
  MriDefineModuleFunction(module, "is_preload_file_cached",
                          em_is_preload_file_cached);
}

}  // namespace binding

#endif  // OS_EMSCRIPTEN
