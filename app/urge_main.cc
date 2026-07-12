// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define SDL_MAIN_USE_CALLBACKS
#include "SDL3/SDL_main.h"

#include <filesystem>

#include "base/debug/logging.h"
#include "components/filesystem/io_service.h"
#include "components/version/version.h"
#include "content/app/runner.h"

struct URGEAppState {
  std::unique_ptr<content::Runner> runner;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  auto* app = new URGEAppState;
  *appstate = app;

  LOG(INFO) << "[Core] Git Revision: " << URGE_GIT_REVISION;
  LOG(INFO) << "[Core] Build Date: " << URGE_BUILD_DATE;

  std::string app_path;
#if defined(OS_ANDROID)
  // Get GAME_PATH string field from JNI (MainActivity.java)
  JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
  jobject activity = (jobject)SDL_GetAndroidActivity();
  jclass activity_klass = env->GetObjectClass(activity);
  jfieldID field_game_path =
      env->GetStaticFieldID(activity_klass, "GAME_PATH", "Ljava/lang/String;");
  jstring java_string_game_path =
      (jstring)env->GetStaticObjectField(activity_klass, field_game_path);
  const char* game_data_dir = env->GetStringUTFChars(java_string_game_path, 0);

  // Set and ensure current directory
  std::filesystem::path std_path(game_data_dir);
  if (!std::filesystem::exists(std_path) ||
      !std::filesystem::is_directory(std_path))
    std::filesystem::create_directories(std_path);

  std::filesystem::current_path(std_path);

  env->ReleaseStringUTFChars(java_string_game_path, game_data_dir);
  env->DeleteLocalRef(java_string_game_path);
  env->DeleteLocalRef(activity_klass);

  // Fixed configure file
  app_path = "Game";
#elif defined(OS_EMSCRIPTEN)
  app_path = "Game";
#else   // OS_WIN, OS_LINUX
  app_path = argv[0];
  for (size_t i = 0; i < app_path.size(); ++i)
    if (app_path[i] == '\\')
      app_path[i] = '/';

  auto last_sep = app_path.find_last_of('/');
  if (last_sep != std::string::npos)
    app_path = app_path.substr(last_sep + 1);

  last_sep = app_path.find_last_of('.');
  if (last_sep != std::string::npos)
    app_path = app_path.substr(0, last_sep);
#endif  //! defined(OS_ANDROID)

  // Current path
  std::u8string current_path =
      std::filesystem::current_path().generic_u8string();
  const char* current_path_string =
      reinterpret_cast<const char*>(current_path.c_str());

  // Filesystem
  auto* filesystem = new filesystem::IOService(argv[0]);
  filesystem->AddLoadPath(current_path_string, "", false);
  filesystem->SetWritePath(current_path_string);
  filesystem::IOService::Reset(filesystem);

  // CoreProfile
  auto config_path = app_path + ".yml";
  filesystem::IOState config_io_state;
  auto config_stream = filesystem->OpenReadRaw(app_path, &config_io_state);

  auto* profile = new content::CoreProfile(config_stream);
  content::CoreProfile::Instance(profile);

  // Binding
  auto binding = std::make_unique<content::ExternalBinding>();

  // Runner
  app->runner = std::make_unique<content::Runner>(std::move(binding));

  return static_cast<SDL_AppResult>(app->runner->AppInit());
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto* app = static_cast<URGEAppState*>(appstate);

  return static_cast<SDL_AppResult>(app->runner->RunIterate());
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto* app = static_cast<URGEAppState*>(appstate);

  return static_cast<SDL_AppResult>(app->runner->ProcessEvent(event));
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  auto* app = static_cast<URGEAppState*>(appstate);

  delete app;
}
