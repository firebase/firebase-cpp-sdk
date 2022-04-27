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

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>  // NOLINT
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _WIN32

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#include "app_framework.h"  // NOLINT

static bool quit = false;

#ifdef _WIN32
static BOOL WINAPI SignalHandler(DWORD event) {
  if (!(event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT)) {
    return FALSE;
  }
  quit = true;
  return TRUE;
}
#else
static void SignalHandler(int /* ignored */) { quit = true; }
#endif  // _WIN32

namespace app_framework {

bool ProcessEvents(int msec) {
#ifdef _WIN32
  Sleep(msec);
#else
  usleep(msec * 1000);
#endif  // _WIN32
  return quit;
}

std::string PathForResource() {
#if defined(_WIN32)
  // On Windows we should hvae TEST_TMPDIR or TEMP or TMP set.
  char buf[MAX_PATH + 1];
  if (GetEnvironmentVariable("TEST_TMPDIR", buf, sizeof(buf)) ||
      GetEnvironmentVariable("TEMP", buf, sizeof(buf)) ||
      GetEnvironmentVariable("TMP", buf, sizeof(buf))) {
    std::string path(buf);
    // Add trailing slash.
    if (path[path.size() - 1] != '\\') path += '\\';
    return path;
  }
#else
  // Linux and OS X should either have the TEST_TMPDIR environment variable set
  // or use /tmp.
  if (const char* value = getenv("TEST_TMPDIR")) {
    std::string path(value);
    // Add trailing slash.
    if (path[path.size() - 1] != '/') path += '/';
    return path;
  }
  struct stat s;
  if (stat("/tmp", &s) == 0) {
    if (s.st_mode & S_IFDIR) {
      return std::string("/tmp/");
    }
  }
#endif  // defined(_WIN32)
  // If nothing else, use the current directory.
  return std::string();
}
void LogMessageV(bool suppress, const char* format, va_list list) {
  // Save the log to the Full Logs list regardless of whether it should be
  // suppressed.
  static const int kLineBufferSize = 1024;
  char buffer[kLineBufferSize + 2];
  int string_len = vsnprintf(buffer, kLineBufferSize, format, list);
  string_len = string_len < kLineBufferSize ? string_len : kLineBufferSize;
  // Append a linebreak to the buffer.
  buffer[string_len] = '\n';
  buffer[string_len + 1] = '\0';
  if (GetPreserveFullLog()) {
    AddToFullLog(buffer);
  }
  if (!suppress) {
    fputs(buffer, stdout);
    fflush(stdout);
  }
}

void LogMessage(const char* format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageV(false, format, list);
  va_end(list);
}

static bool g_save_full_log = false;
static std::vector<std::string> g_full_logs;  // NOLINT
static std::mutex g_full_log_mutex;

void AddToFullLog(const char* str) {
  std::lock_guard<std::mutex> guard(g_full_log_mutex);
  g_full_logs.push_back(std::string(str));
}

bool GetPreserveFullLog() { return g_save_full_log; }
void SetPreserveFullLog(bool b) { g_save_full_log = b; }

void ClearFullLog() {
  std::lock_guard<std::mutex> guard(g_full_log_mutex);
  g_full_logs.clear();
}

void OutputFullLog() {
  std::lock_guard<std::mutex> guard(g_full_log_mutex);
  for (int i = 0; i < g_full_logs.size(); ++i) {
    fputs(g_full_logs[i].c_str(), stdout);
  }
  fflush(stdout);
  g_full_logs.clear();
}

WindowContext GetWindowContext() { return nullptr; }
WindowContext GetWindowController() { return nullptr; }

// Change the current working directory to the directory containing the
// specified file.
void ChangeToFileDirectory(const char* file_path) {
  std::string path(file_path);
  std::replace(path.begin(), path.end(), '\\', '/');
  auto slash = path.rfind('/');
  if (slash != std::string::npos) {
    std::string directory = path.substr(0, slash);
    if (!directory.empty()) {
      LogDebug("chdir %s", directory.c_str());
      chdir(directory.c_str());
    }
  }
}

#if defined(_WIN32)  // The other platforms are implemented in app_framework.cc.
// Returns the number of microseconds since the epoch.
int64_t GetCurrentTimeInMicroseconds() {
  FILETIME file_time;
  GetSystemTimeAsFileTime(&file_time);

  ULARGE_INTEGER now;
  now.LowPart = file_time.dwLowDateTime;
  now.HighPart = file_time.dwHighDateTime;

  // Windows file time is expressed in 100s of nanoseconds.
  // To convert to microseconds, multiply x10.
  return now.QuadPart * 10LL;
}
#endif  // defined(_WIN32)

void RunOnBackgroundThread(void* (*func)(void*), void* data) {
  // On desktop, use std::thread as Windows doesn't support pthreads.
  std::thread thread(func, data);
  thread.detach();
}

std::string ReadTextInput(const char* title, const char* message,
                          const char* placeholder) {
  if (title && *title) {
    int len = strlen(title);
    printf("\n");
    for (int i = 0; i < len; ++i) {
      printf("=");
    }
    printf("\n%s\n", title);
    for (int i = 0; i < len; ++i) {
      printf("=");
    }
  }
  printf("\n%s", message);
  if (placeholder && *placeholder) {
    printf(" [%s]", placeholder);
  }
  printf(": ");
  fflush(stdout);
  std::string input_line;
  std::getline(std::cin, input_line);
  return input_line.empty() ? std::string(placeholder) : input_line;
}

bool ShouldRunUITests() { return true; }

bool ShouldRunNonUITests() { return true; }

bool IsLoggingToFile() { return false; }

}  // namespace app_framework

int main(int argc, char* argv[]) {
#ifdef _WIN32
  SetConsoleCtrlHandler((PHANDLER_ROUTINE)SignalHandler, TRUE);
#else
  signal(SIGINT, SignalHandler);
#endif  // _WIN32
  return common_main(argc, argv);
}
