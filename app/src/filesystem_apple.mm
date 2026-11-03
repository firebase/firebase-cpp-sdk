// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "app/src/filesystem.h"

#import <Foundation/Foundation.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "app/src/util.h"

#if __APPLE__
#include "TargetConditionals.h"
#endif

namespace firebase {

namespace {

bool Mkdir(const std::string& path, std::string* out_error) {
  int retval = mkdir(path.c_str(), 0700);
  if (retval != 0) {
    int err = errno;
    if (err != EEXIST) {
      if (out_error) {
        *out_error = std::string("Could not create directory ") + path +
                     " (error code: " + std::to_string(err) + ")";
      }

      return false;
    }
  }

  return true;
}

}  // namespace

std::string AppDataDir(const char* app_name, bool should_create, std::string* out_error) {
  if (!app_name || std::strlen(app_name) == 0) {
    if (out_error) {
      *out_error = "AppDataDir failed: no app_name provided";
    }
    return "";
  }

  std::string path;
  @autoreleasepool {
#if TARGET_OS_IOS || TARGET_OS_OSX
    NSArray<NSString*>* directories =
        NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
#elif TARGET_OS_TV
    NSArray<NSString*>* directories =
        NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
#else
#error "Don't know where to store documents on this platform."
#endif

    const char* base_dir_cstr =
        directories.count > 0 ? directories[0].UTF8String : nullptr;
    if (base_dir_cstr == nullptr) {
      if (out_error) {
        *out_error = "AppDataDir failed: could not find application data directory";
      }
      return "";
    }
    path = base_dir_cstr;
  }

  if (should_create) {
    if (!Mkdir(path, out_error)) {
      return "";
    }
    // App name might contain path separators. Split it to get list of subdirs.
    for (const std::string& nested_dir : SplitString(app_name, '/')) {
      if (nested_dir.empty()) {
        continue;
      }
      path += "/";
      path += nested_dir;
      bool created = Mkdir(path, out_error);
      if (!created) return "";
    }

    return path;
  } else {
    return path + "/" + app_name;
  }
}

}  // namespace firebase
