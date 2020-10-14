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

#import <Foundation/Foundation.h>

#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

std::string GetAppDataPath(const char* app_name, bool should_create) {
  NSArray<NSString*>* directories =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
  std::string path = directories[0].UTF8String;
  if (should_create) {
    int retval;
    retval = mkdir(path.c_str(), 0700);
    if (retval != 0 && errno != EEXIST) return "";
    // App name might contain path separators. Split it to get list of subdirs
    std::vector<std::string> app_name_parts = SplitString(app_name, '/');
    if (app_name_parts.empty()) return "";
    std::string dir_path = path;
    // Recursively create entire tree of directories
    for (std::vector<std::string>::const_iterator it = app_name_parts.begin();
         it != app_name_parts.end(); it++) {
      dir_path = dir_path + "/" + *it;
      retval = mkdir(dir_path.c_str(), 0700);
      if (retval != 0 && errno != EEXIST) return "";
    }
  }
  return path + "/" + app_name;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
