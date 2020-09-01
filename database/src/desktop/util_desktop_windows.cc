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

#include <direct.h>
#include <shlobj.h>
#include <stringapiset.h>

#include <iostream>
#include <string>

#include "app/src/log.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

// Split a string based on specified character delimiter into constituent parts
static std::vector<std::string> split_string(const std::string& s,
                                             const char delimiter='/') {
  size_t pos = 0;
  // This index is used as the starting index to search the delimiters from.
  size_t delimiter_search_start = 0;
  // Skip any leading delimiters
  while(s[delimiter_search_start] == delimiter) {
    delimiter_search_start++;
  }

  std::vector<std::string> split_parts;
  size_t len = s.size();
  // Cant proceed if input string consists of just delimiters
  if (pos >= len) {
    return split_parts;
  }

  while( (pos = s.find(delimiter, delimiter_search_start)) != std::string::npos) {
    split_parts.push_back(s.substr(delimiter_search_start, pos-delimiter_search_start));

    while(s[pos] == delimiter && pos<len) {
      pos++;
      delimiter_search_start = pos;
    }
  }

  // If the input string doesn't end with a delimiter we need to push the last
  // token into our return vector
  if (delimiter_search_start != len)
    split_parts.push_back(s.substr(delimiter_search_start, len-delimiter_search_start));

  return split_parts;
}

static std::string utf8_encode(const std::wstring& wstr) {
  if (wstr.empty()) return std::string();
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                        NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0],
                      size_needed, NULL, NULL);
  return strTo;
}

std::string GetAppDataPath(const char* app_name, bool should_create) {
  wchar_t* pwstr = nullptr;
  HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &pwstr);
  if (result != S_OK) {
    return "";
  }
  std::wstring wstr(pwstr);
  CoTaskMemFree(static_cast<void*>(pwstr));
  std::string path = utf8_encode(wstr);
  if (path == "") return "";
  if (should_create) {
    // App name might contain path separators. Split it to get list of subdirs
    std::vector<std::string> app_name_parts = split_string(app_name, '/');
    if(app_name_parts.empty()) return "";
    std::string dir_path = path;
    // Recursively create the entire tree of directories
    for (std::vector<std::string>::const_iterator it = app_name_parts.begin();
                                            it != app_name_parts.end(); it++) {
          dir_path = dir_path + "\\" + *it;
          retval = _mkdir(dir_path.c_str());
          if (retval != 0 && errno != EEXIST) return "";
    }
  }
  return path + "\\" + app_name;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
