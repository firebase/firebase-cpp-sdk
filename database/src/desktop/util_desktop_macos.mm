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

#include <sys/stat.h>
#include <sys/types.h>
#include "database/src/desktop/util_desktop.h"
#include <string>

namespace firebase {
namespace database {
namespace internal {

// Given a directory path, and a directory creation mode with an option delimiter,
// recursively create directories. mkdir doesn't create directories recursively
// This is similar to `mkdir -p` on the command line
static int mkdir_recursive(const char* dir_path, const mode_t mode) {
  std::string dir_path_string(dir_path);
  const char path_delimiter = '/';

  // This index will be used to track the positions of path_delimiters in the path string
  size_t pos = 0;
  // This index is used as the starting index to search the path_delimiters from.
  size_t path_delimiter_search_start = 0;
  // Skip any leading path_delimiters
  while(dir_path_string[path_delimiter_search_start] == path_delimiter) {
    path_delimiter_search_start++;
  }
  size_t len = dir_path_string.size();
  // Invalid path if it consists of just path_delimiters
  if (pos >= len) {
    return -1;
  }
  std::string sub_directory;
  int retval;
  while( (pos = dir_path_string.find(path_delimiter, path_delimiter_search_start))
                                                             != std::string::npos) {
    sub_directory = dir_path_string.substr(0, pos);
    retval = mkdir(sub_directory.c_str(), mode);
    if (retval != 0 && errno != EEXIST) return -1;
    while(dir_path_string[pos] == path_delimiter && pos<len) {
      pos++;
      path_delimiter_search_start = pos;
    }
  }

  // If the path does not have trailing delimiter, we still have to create the
  // last subdirectory
  if (dir_path_string[len-1] != path_delimiter) {
    retval = mkdir(dir_path_string.c_str(), mode);
    if (retval != 0 && errno != EEXIST) return -1;
  }
  return 0;
}


std::string GetAppDataPath(const char* app_name, bool should_create) {
  NSArray<NSString*>* directories =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
  std::string path = directories[0].UTF8String;
  if (should_create) {
    int retval;
    retval = mkdir(path.c_str(), 0700);
    if (retval != 0 && errno != EEXIST) return "";
    // app_name could have a path delimiter "/" in it. Use custom recursive mkdir.
    retval = mkdir_recursive((path + "/" + app_name).c_str(), 0700);
    if (retval != 0 && errno != EEXIST) return "";
  }
  return path + "/" + app_name;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
