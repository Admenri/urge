#include "app/platform/macos/native_app.h"

#import <AppKit/AppKit.h>

#include <atomic>
#include <cstring>
#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "urge_icon.h"

namespace {
std::string gExecutable;
std::vector<std::string> gArguments;
std::string gPendingGame;

bool IsGameFolder(NSURL* folder) {
  BOOL directory = NO;
  NSFileManager* files = [NSFileManager defaultManager];
  return [files fileExistsAtPath:folder.path isDirectory:&directory] && directory &&
         [files fileExistsAtPath:[[folder URLByAppendingPathComponent:@"Game.ini"] path]];
}

void ShowGameError(NSString* message) {
  NSAlert* alert = [[NSAlert alloc] init];
  alert.messageText = @"Unable to open game";
  alert.informativeText = message;
  [alert runModal];
}
}

@interface URGEGameMenuController : NSObject
- (void)openGame:(id)sender;
@end

@implementation URGEGameMenuController
- (void)openGame:(id)sender {
  if (!gPendingGame.empty())
    return;
  NSOpenPanel* panel = [NSOpenPanel openPanel];
  panel.canChooseFiles = NO;
  panel.canChooseDirectories = YES;
  panel.allowsMultipleSelection = NO;
  panel.message = @"Choose a game folder containing Game.ini.";
  panel.prompt = @"Open Game";
  if ([panel runModal] != NSModalResponseOK)
    return;
  NSURL* game = panel.URL.URLByResolvingSymlinksInPath;
  if (!IsGameFolder(game)) {
    ShowGameError(@"The selected folder does not contain Game.ini.");
    return;
  }
  gPendingGame = game.fileSystemRepresentation;
  SDL_Event event = {};
  event.type = SDL_EVENT_QUIT;
  if (!SDL_PushEvent(&event)) {
    gPendingGame.clear();
    ShowGameError(@"The current game could not be stopped. Please try again.");
  }
}
@end

namespace platform::macos {

std::string PrepareBundledGame(int argc, char* argv[], std::string& error) {
  @autoreleasepool {
    NSBundle* bundle = [NSBundle mainBundle];
    if (![bundle.bundleIdentifier isEqualToString:@"com.sxysxy.urge.native"])
      return {};
    gExecutable = bundle.executableURL.fileSystemRepresentation;
    gArguments = {gExecutable};
    std::string selected;
    for (int i = 1; i < argc; ++i) {
      if (std::strcmp(argv[i], "--game") == 0) {
        if (++i == argc) {
          error = "--game requires a game directory";
          return {};
        }
        selected = argv[i];
      } else {
        gArguments.emplace_back(argv[i]);
      }
    }
    if (!selected.empty()) {
      NSURL* folder = [NSURL fileURLWithPath:[NSString stringWithUTF8String:selected.c_str()]
                               isDirectory:YES].URLByResolvingSymlinksInPath;
      if (!IsGameFolder(folder)) {
        error = "The selected game directory does not contain Game.ini";
        return {};
      }
      return std::string(folder.fileSystemRepresentation);
    }
    NSFileManager* files = [NSFileManager defaultManager];
    NSURL* adjacent = [bundle.bundleURL URLByDeletingLastPathComponent];
    if ([files fileExistsAtPath:[[adjacent URLByAppendingPathComponent:@"Game.ini"] path]])
      return std::string(adjacent.fileSystemRepresentation);

    NSURL* support = [[files URLsForDirectory:NSApplicationSupportDirectory
                                   inDomains:NSUserDomainMask] firstObject];
    NSURL* game = [support URLByAppendingPathComponent:@"URGE/NativeDemo" isDirectory:YES];
    NSError* failure = nil;
    if (!game || ![files createDirectoryAtURL:[game URLByAppendingPathComponent:@"Data" isDirectory:YES]
                 withIntermediateDirectories:YES attributes:nil error:&failure]) {
      error = failure ? failure.localizedDescription.UTF8String : "Cannot locate Application Support";
      return {};
    }
    NSURL* demo = [bundle.resourceURL URLByAppendingPathComponent:@"Demo" isDirectory:YES];
    for (NSString* name in @[@"Game.ini", @"Data/Scripts.rxdata"]) {
      NSURL* destination = [game URLByAppendingPathComponent:name];
      // Keep user display/input preferences; refresh only the bundled script.
      if ([name isEqualToString:@"Game.ini"] && [files fileExistsAtPath:destination.path])
        continue;
      NSData* source = [NSData dataWithContentsOfURL:[demo URLByAppendingPathComponent:name]
                                           options:0 error:&failure];
      if (!source || (![source isEqualToData:[NSData dataWithContentsOfURL:destination]] &&
                      ![source writeToURL:destination options:NSDataWritingAtomic error:&failure])) {
        error = failure ? failure.localizedDescription.UTF8String : "Cannot prepare bundled demo";
        return {};
      }
    }
    return std::string(game.fileSystemRepresentation);
  }
}

void InstallGameMenu() {
  if (gExecutable.empty())
    return;  // The standalone Game executable keeps SDL's standard menu.
  static URGEGameMenuController* controller = [[URGEGameMenuController alloc] init];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"File"];
  NSMenuItem* open = [[NSMenuItem alloc] initWithTitle:@"Open Game…"
                                             action:@selector(openGame:) keyEquivalent:@"o"];
  open.target = controller;
  [menu addItem:open];
  NSMenuItem* file = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
  file.submenu = menu;
  [NSApp.mainMenu insertItem:file atIndex:MIN(1, NSApp.mainMenu.numberOfItems)];
}

bool RelaunchSelectedGame() {
  if (gPendingGame.empty())
    return true;
  gArguments.emplace_back("--game");
  gArguments.push_back(gPendingGame);
  NSMutableArray<NSString*>* arguments = [NSMutableArray array];
  for (std::size_t i = 1; i < gArguments.size(); ++i)
    [arguments addObject:[NSString stringWithUTF8String:gArguments[i].c_str()]];
  SDL_Quit();
  // LaunchServices must register a new process. Re-execing a Cocoa app in place
  // leaves its file-dialog/activation services associated with the old image.
  NSWorkspaceOpenConfiguration* configuration = [NSWorkspaceOpenConfiguration configuration];
  configuration.arguments = arguments;
  configuration.createsNewApplicationInstance = YES;
  configuration.activates = YES;
  std::atomic<bool> finished{false};
  auto* completion = &finished;
  __block NSError* launchError = nil;
  [[NSWorkspace sharedWorkspace] openApplicationAtURL:[NSBundle mainBundle].bundleURL
      configuration:configuration
      completionHandler:^(NSRunningApplication*, NSError* error) {
        launchError = error;
        completion->store(true, std::memory_order_release);
      }];
  while (!finished.load(std::memory_order_acquire))
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
  if (launchError) {
    ShowGameError(launchError.localizedDescription);
    return false;
  }
  return true;
}

void SetDefaultApplicationIcon() {
  @autoreleasepool {
    NSData* data = [NSData dataWithBytes:kUrgeIconPng length:sizeof(kUrgeIconPng)];
    NSImage* icon = [[NSImage alloc] initWithData:data];
    if (icon)
      [NSApp setApplicationIconImage:icon];
  }
}

}
