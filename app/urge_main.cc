
#include <iostream>

#include "binding/unittests/binding_unittests.h"
#include "components/filesystem/filesystem.h"
#include "content/worker/worker_scheduler.h"
#include "ui/widget/widget.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_image/SDL_image.h"
#include "SDL3_ttf/SDL_ttf.h"

namespace {

void ReplaceStringWidth(std::string& str, char before, char after) {
  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] == before)
      str[i] = after;
}

}  // namespace

int SDL_main(int argc, char** argv) {
#if defined(OS_WIN)
  ::SetConsoleOutputCP(CP_UTF8);
#endif  //! defined(OS_WIN)

  std::string app(argv[0]);
  ReplaceStringWidth(app, '\\', '/');
  auto last_sep = app.find_last_of('/');
  if (last_sep != std::string::npos)
    app = app.substr(last_sep + 1);

  content::Debug() << "[App] Path: " << app;

  last_sep = app.find_last_of('.');
  if (last_sep != std::string::npos)
    app = app.substr(0, last_sep);
  std::string ini = app + ".ini";

  std::unique_ptr<filesystem::Filesystem> io =
      std::make_unique<filesystem::Filesystem>(argv[0]);
  io->AddLoadPath(".");

  SDL_IOStream* inifile = nullptr;
  try {
    inifile = io->OpenReadRaw(ini);
  } catch (const base::Exception& e) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RGU Kernel",
                             e.GetErrorMessage().c_str(), nullptr);
    return 1;
  }

  scoped_refptr<content::CoreConfigure> config = new content::CoreConfigure;
  config->LoadCommandLine(argc, argv);
  config->LoadConfigure(inifile, app);

  config->executable_file() = app;
  for (auto& it : config->load_paths())
    io->AddLoadPath(it);

  std::unique_ptr<ui::Widget> widget = std::make_unique<ui::Widget>();
  ui::Widget::InitParams window_params;
  window_params.size = config->initial_resolution();
  window_params.title = config->game_title();
  window_params.resizable = true;
  widget->Init(std::move(window_params));

  SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");

  SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  TTF_Init();

  content::ContentParams params;
  params.binding = std::make_unique<binding::BindingUnittests>();
  params.config = config;
  params.file_io = io.get();
  params.window = std::move(widget);

  content::WorkerScheduler ws;
  ws.Init(std::move(params));
  ws.Run();

  TTF_Quit();
  IMG_Quit();
  SDL_Quit();

  return 0;
}
