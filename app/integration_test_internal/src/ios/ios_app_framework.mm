// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import <UIKit/UIKit.h>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "app_framework.h"

@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property(nonatomic, strong) UIWindow *window;

@end

@interface FTAViewController : UIViewController

@property(atomic, strong) NSString *textEntryResult;

@end

static NSString *const kGameLoopUrlPrefix = @"firebase-game-loop";
static NSString *const kUITestUrlPrefix = @"firebase-ui-test";
static NSString *const kGameLoopCompleteUrlScheme = @"firebase-game-loop-complete://";
static const float kGameLoopSecondsToPauseBeforeQuitting = 5.0f;

// Test Loop on iOS doesn't provide the app under test a path to save logs to, so set it here.
#define GAMELOOP_DEFAULT_LOG_FILE "Results1.json"

enum class RunningState { kRunning, kShuttingDown, kShutDown };

// Note: g_running_state and g_exit_status must only be accessed while holding the lock from
// g_running_state_condition; also, any changes to these values should be followed up with a
// call to [g_running_state_condition broadcast].
static NSCondition *g_running_state_condition;
static RunningState g_running_state = RunningState::kRunning;
static int g_exit_status = -1;

static UITextView *g_text_view;
static UIView *g_parent_view;
static UIViewController *g_parent_view_controller;
static FTAViewController *g_view_controller;
static bool g_gameloop_launch = false;
static bool g_uitest_launch = false;
static NSURL *g_results_url;
static NSString *g_file_name;
static NSString *g_file_url_path;

@implementation FTAViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  g_parent_view = self.view;
  g_parent_view_controller = nil;
  UIResponder *responder = [g_parent_view nextResponder];
  while (responder != nil) {
    if ([responder isKindOfClass:[UIViewController class]]) {
      g_parent_view_controller = (UIViewController *)responder;
      break;
    }
    responder = [responder nextResponder];
  }

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    // Copy the app name into a non-const array, as googletest requires that
    // main() take non-const char* argv[] so it can modify the arguments.
    char *argv[1];
    argv[0] = new char[strlen(TESTAPP_NAME) + 1];
    strcpy(argv[0], TESTAPP_NAME);  // NOLINT
    int common_main_result = common_main(1, argv);

    [g_running_state_condition lock];
    g_exit_status = common_main_result;
    g_running_state = RunningState::kShutDown;
    [g_running_state_condition broadcast];
    [g_running_state_condition unlock];

    delete[] argv[0];
    argv[0] = nullptr;
    [NSThread sleepForTimeInterval:kGameLoopSecondsToPauseBeforeQuitting];
    dispatch_async(dispatch_get_main_queue(), ^{
      [UIApplication.sharedApplication openURL:[NSURL URLWithString:kGameLoopCompleteUrlScheme]
                                       options:[NSDictionary dictionary]
                             completionHandler:nil];
    });
  });
}

@end
namespace app_framework {

bool ProcessEvents(int msec) {
  NSDate *endDate = [NSDate dateWithTimeIntervalSinceNow:static_cast<float>(msec) / 1000.0f];
  [g_running_state_condition lock];

  if (g_running_state == RunningState::kRunning) {
    [g_running_state_condition waitUntilDate:endDate];
  }

  RunningState running_status = g_running_state;
  [g_running_state_condition unlock];

  return running_status != RunningState::kRunning;
}

std::string PathForResource() {
  NSArray<NSString *> *paths =
      NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirectory = paths.firstObject;
  // Force a trailing slash by removing any that exists, then appending another.
  return std::string(
      [[documentsDirectory stringByStandardizingPath] stringByAppendingString:@"/"].UTF8String);
}

WindowContext GetWindowContext() { return g_parent_view; }
WindowContext GetWindowController() { return g_parent_view_controller; }

// Log a message that can be viewed in the console.
void LogMessageV(bool suppress, const char *format, va_list list) {
  NSString *formatString = @(format);

  NSString *message = [[NSString alloc] initWithFormat:formatString arguments:list];
  message = [message stringByAppendingString:@"\n"];

  if (GetPreserveFullLog()) {
    AddToFullLog(message.UTF8String);
  }
  if (!suppress) {
    fputs(message.UTF8String, stdout);
    fflush(stdout);
  }
}

void LogMessage(const char *format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageV(false, format, list);
  va_end(list);
}

static bool g_save_full_log = false;
static std::vector<std::string> g_full_logs;  // NOLINT

void AddToFullLog(const char *str) { g_full_logs.push_back(std::string(str)); }

bool GetPreserveFullLog() { return g_save_full_log; }
void SetPreserveFullLog(bool b) { g_save_full_log = b; }

void ClearFullLog() { g_full_logs.clear(); }

void OutputFullLog() {
  for (int i = 0; i < g_full_logs.size(); ++i) {
    fputs(g_full_logs[i].c_str(), stdout);
  }
  fflush(stdout);
  ClearFullLog();
}

// Log a message that can be viewed in the console.
void AddToTextView(const char *str) {
  NSString *message = @(str);

  dispatch_async(dispatch_get_main_queue(), ^{
    g_text_view.text = [g_text_view.text stringByAppendingString:message];
    NSRange range = NSMakeRange(g_text_view.text.length, 0);
    [g_text_view scrollRangeToVisible:range];
  });
  if (g_gameloop_launch || g_uitest_launch) {
    NSData *data = [message dataUsingEncoding:NSUTF8StringEncoding];
    if ([NSFileManager.defaultManager fileExistsAtPath:g_file_url_path]) {
      NSFileHandle *fileHandler = [NSFileHandle fileHandleForUpdatingAtPath:g_file_url_path];
      [fileHandler seekToEndOfFile];
      [fileHandler writeData:data];
      [fileHandler closeFile];
    } else {
      NSLog(@"Write to file %@", g_file_url_path);
      [data writeToFile:g_file_url_path atomically:YES];
    }
  }
}

// Remove all lines starting with these strings.
static const char *const filter_lines[] = {nullptr};

bool should_filter(const char *str) {
  for (int i = 0; filter_lines[i] != nullptr; ++i) {
    if (strncmp(str, filter_lines[i], strlen(filter_lines[i])) == 0) return true;
  }
  return false;
}

void *stdout_logger(void *filedes_ptr) {
  int fd = reinterpret_cast<int *>(filedes_ptr)[0];
  std::string buffer;
  char bufchar;
  while (size_t n = read(fd, &bufchar, 1)) {
    if (bufchar == '\0') {
      break;
    } else if (bufchar == '\n') {
      if (!should_filter(buffer.c_str())) {
        NSLog(@"%s", buffer.c_str());
        buffer = buffer + bufchar;  // Add the newline
        app_framework::AddToTextView(buffer.c_str());
      }
      buffer.clear();
    } else {
      buffer = buffer + bufchar;
    }
  }
  return nullptr;
}

void RunOnBackgroundThread(void *(*func)(void *), void *data) {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    func(data);
  });
}

// Create an alert dialog via UIAlertController, and prompt the user to enter a line of text.
// This function spins until the text has been entered (or the alert dialog was canceled).
// If the user cancels, returns an empty string.
std::string ReadTextInput(const char *title, const char *message, const char *placeholder) {
  assert(g_view_controller);
  // This should only be called from a background thread, as it blocks, which will mess up the main
  // thread.
  assert(![NSThread isMainThread]);

  g_view_controller.textEntryResult = nil;

  dispatch_async(dispatch_get_main_queue(), ^{
    UIAlertController *alertController =
        [UIAlertController alertControllerWithTitle:@(title)
                                            message:@(message)
                                     preferredStyle:UIAlertControllerStyleAlert];
    [alertController addTextFieldWithConfigurationHandler:^(UITextField *_Nonnull textField) {
      textField.placeholder = @(placeholder);
    }];
    UIAlertAction *confirmAction = [UIAlertAction
        actionWithTitle:@"OK"
                  style:UIAlertActionStyleDefault
                handler:^(UIAlertAction *_Nonnull action) {
                  g_view_controller.textEntryResult = alertController.textFields.firstObject.text;
                }];
    [alertController addAction:confirmAction];
    UIAlertAction *cancelAction = [UIAlertAction actionWithTitle:@"Cancel"
                                                           style:UIAlertActionStyleCancel
                                                         handler:^(UIAlertAction *_Nonnull action) {
                                                           g_view_controller.textEntryResult = @"";
                                                         }];
    [alertController addAction:cancelAction];
    [g_view_controller presentViewController:alertController animated:YES completion:nil];
  });

  while (true) {
    // Pause a second, waiting for the user to enter text.
    if (ProcessEvents(1000)) {
      // If this returns true, an exit was requested.
      return "";
    }
    if (g_view_controller.textEntryResult != nil) {
      // textEntryResult will be set to non-nil when a dialog button is clicked.
      std::string result = g_view_controller.textEntryResult.UTF8String;
      g_view_controller.textEntryResult = nil;  // Consume the result.
      return result;
    }
  }
}

bool ShouldRunUITests() { return !g_gameloop_launch; }

bool ShouldRunNonUITests() { return !g_uitest_launch; }

bool IsLoggingToFile() { return g_file_url_path; }

bool StartLoggingToFile(const char *file_path) {
  NSURL *home_url = [NSURL fileURLWithPath:NSHomeDirectory()];
  g_results_url = [home_url URLByAppendingPathComponent:@"/Documents/GameLoopResults"];
  g_file_name = @(file_path);
  g_file_url_path = [g_results_url URLByAppendingPathComponent:g_file_name].path;
  NSError *error;
  if (![NSFileManager.defaultManager fileExistsAtPath:[g_results_url path]]) {
    if (![NSFileManager.defaultManager createDirectoryAtPath:g_results_url.path
                                 withIntermediateDirectories:true
                                                  attributes:nil
                                                       error:&error]) {
      app_framework::LogError("Couldn't create directory %s: %s", g_results_url.path,
                              error.description.UTF8String);
      g_file_url_path = nil;
      return false;
    }
  }
  return true;
}

}  // namespace app_framework

int main(int argc, char *argv[]) {
  // Pipe stdout to call LogToTextView so we can see the gtest output.
  int filedes[2];
  assert(pipe(filedes) != -1);
  assert(dup2(filedes[1], STDOUT_FILENO) != -1);
  pthread_t thread;
  pthread_create(&thread, nullptr, app_framework::stdout_logger, reinterpret_cast<void *>(filedes));
  @autoreleasepool {
    UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
  // Signal to stdout_logger to exit.
  write(filedes[1], "\0", 1);
  pthread_join(thread, nullptr);
  close(filedes[0]);
  close(filedes[1]);

  int exit_status = -1;
  [g_running_state_condition lock];
  exit_status = g_exit_status;
  [g_running_state_condition unlock];

  NSLog(@"Application Exit");
  return exit_status;
}

@implementation AppDelegate
- (BOOL)application:(UIApplication *)app
            openURL:(NSURL *)url
            options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options {
#if TARGET_OS_SIMULATOR
  setenv("USE_FIRESTORE_EMULATOR", "true", 1);
#endif

  if ([url.scheme isEqual:kGameLoopUrlPrefix]) {
    g_gameloop_launch = true;
    app_framework::StartLoggingToFile(GAMELOOP_DEFAULT_LOG_FILE);
    return YES;
  } else if ([url.scheme isEqual:kUITestUrlPrefix]) {
    g_uitest_launch = true;
    app_framework::StartLoggingToFile(GAMELOOP_DEFAULT_LOG_FILE);
    return YES;
  }
  NSLog(@"The testapp will not log to files since it is not launched by URL %@ or %@",
        kGameLoopUrlPrefix, kUITestUrlPrefix);
  return NO;
}

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  g_running_state_condition = [[NSCondition alloc] init];

  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  g_view_controller = [[FTAViewController alloc] init];
  self.window.rootViewController = g_view_controller;
  [self.window makeKeyAndVisible];

  g_text_view = [[UITextView alloc] initWithFrame:g_view_controller.view.bounds];

  g_text_view.accessibilityIdentifier = @"Logger";
#if TARGET_OS_IOS
  g_text_view.editable = NO;
#endif  // TARGET_OS_IOS
  g_text_view.scrollEnabled = YES;
  g_text_view.userInteractionEnabled = YES;
  g_text_view.font = [UIFont fontWithName:@"Courier" size:10];
  [g_view_controller.view addSubview:g_text_view];

  return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application {
  [g_running_state_condition lock];

  if (g_running_state == RunningState::kRunning) {
    g_running_state = RunningState::kShuttingDown;
    [g_running_state_condition broadcast];
  }

  while (g_running_state != RunningState::kShutDown) {
    [g_running_state_condition wait];
  }

  [g_running_state_condition unlock];
}
@end
