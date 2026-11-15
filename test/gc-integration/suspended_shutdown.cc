// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Drive the real host callbacks without letting SDL own the test's main loop.
#define SDL_MAIN_HANDLED 1
#include "app/app.cc"
#ifdef main
#undef main
#endif

#include <cassert>
#include <cstdio>

int main(int argc, char** argv) {
  SDL_SetMainReady();
  void* appstate = nullptr;
  const auto initialized = SDL_AppInit(&appstate, argc, argv);
  if (initialized != SDL_APP_CONTINUE) {
    SDL_AppQuit(appstate, initialized);
    SDL_Quit();
    return 1;
  }

  // The packed test script runs rgss_stop (or a rescuing update loop) forever.
  // Stop the host while that Ruby execution stack is suspended at presentation.
  for (int i = 0; i < 20 && urge::Graphics::Get().frame_rate() > 0; ++i) {
    SDL_Delay(20);
    if (SDL_AppIterate(appstate) != SDL_APP_CONTINUE)
      break;
    if (*urge::Graphics::Get().Attr_FrameCount() >= 2)
      break;
  }
  assert(*urge::Graphics::Get().Attr_FrameCount() >= 1);
  SDL_AppQuit(appstate, SDL_APP_SUCCESS);
  SDL_Quit();

  FILE* result = std::fopen("shutdown-result.txt", "rb");
  if (!result)
    return 2;
  char text[128] = {};
  const auto size = std::fread(text, 1, sizeof(text) - 1, result);
  std::fclose(result);
  if (size == 0 || std::string_view(text).find("ENSURE_COMPLETE") == std::string_view::npos)
    return 3;
  std::puts("suspended host shutdown passed");
  return 0;
}
