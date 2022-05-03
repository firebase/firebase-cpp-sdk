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

#ifndef APP_FRAMEWORK_H_  // NOLINT
#define APP_FRAMEWORK_H_  // NOLINT

#include <cstdarg>
#include <string>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif  // defined(__APPLE__)

#if !defined(_WIN32)
#include <sys/time.h>
#endif
#if defined(__ANDROID__)
#include <android/native_activity.h>
#include <jni.h>
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
extern "C" {
#include <objc/objc.h>
}  // extern "C"
#endif  // platforms

// Defined using -DTESTAPP_NAME=some_app_name when compiling this
// file.
#ifndef TESTAPP_NAME
#define TESTAPP_NAME "android_main"
#endif  // TESTAPP_NAME

extern "C" int common_main(int argc, char* argv[]);

namespace app_framework {

// Platform-independent logging methods.
enum LogLevel { kDebug = 0, kInfo, kWarning, kError };
void SetLogLevel(LogLevel log_level);
LogLevel GetLogLevel();
void LogError(const char* format, ...);
void LogWarning(const char* format, ...);
void LogInfo(const char* format, ...);
void LogDebug(const char* format, ...);

// Set this to true to have all log messages saved regardless of loglevel; you
// can output them later via OutputFullLog or clear them via ClearFullLog.
void SetPreserveFullLog(bool b);
// Get the value previously set by SetPreserveFullLog.
bool GetPreserveFullLog();

// Add a line of text to the "full log" to be output via OutputFullLog.
void AddToFullLog(const char* str);

// Clear the logs that were saved.
void ClearFullLog();

// Output the full saved logs (if you SetPreserveFullLog(true) earlier).
void OutputFullLog();

// Platform-independent method to flush pending events for the main thread.
// Returns true when an event requesting program-exit is received.
bool ProcessEvents(int msec);

// Returns a path to a writable directory for the given platform.
std::string PathForResource();

// Returns the number of microseconds since the epoch.
int64_t GetCurrentTimeInMicroseconds();

// On desktop, change the current working directory to the directory
// containing the specified file. On mobile, this is a no-op.
void ChangeToFileDirectory(const char* file_path);

// Return whether the file exists.
bool FileExists(const char* file_path);

// WindowContext represents the handle to the parent window. Its type
// (and usage) vary based on the OS.
#if defined(__ANDROID__)
typedef jobject WindowContext;  // A jobject to the Java Activity.
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
typedef id WindowContext;  // A pointer to an iOS UIView.
#else
typedef void* WindowContext;  // A void* for any other environments.
#endif

#if defined(__ANDROID__)
// Get the JNI environment.
JNIEnv* GetJniEnv();
// Get the activity.
jobject GetActivity();
// Find a class, attempting to load the class if it's not found.
jclass FindClass(JNIEnv* env, jobject activity_object, const char* class_name);
#endif  // defined(__ANDROID__)

// Returns ture if we run tests that require interaction.
bool ShouldRunUITests();

// Returns true if we run tests that do not require interaction.
bool ShouldRunNonUITests();

// Returns true if the logger is currently logging to a file.
bool IsLoggingToFile();

// Start logging to the given file. You only need to do this if the app has been
// restarted since it was initially run in test loop mode.
bool StartLoggingToFile(const char* path);

// Returns a variable that describes the window context for the app. On Android
// this will be a jobject pointing to the Activity. On iOS, it's an id pointing
// to the root view of the view controller.
WindowContext GetWindowContext();

// Returns a variable that describes the controller of the app's UI. On Android
// this will be a jobject pointing to the Activity, the same as
// GetWindowContext(). On iOS, it's an id pointing to the UIViewController of
// the parent UIView.
WindowContext GetWindowController();

// Run the given function on a detached background thread.
void RunOnBackgroundThread(void* (*func)(void* data), void* data);

// Prompt the user with a dialog box to enter a line of text, blocking
// until the user enters the text or the dialog box is canceled.
// Returns the text that was entered, or an empty string if the user
// canceled.
std::string ReadTextInput(const char* title, const char* message,
                          const char* placeholder);

}  // namespace app_framework

#endif  // APP_FRAMEWORK_H_  // NOLINT
