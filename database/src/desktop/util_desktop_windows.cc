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
  std::string dir = utf8_encode(wstr);
  if (dir == "") return "";
  if (should_create) {
    int retval = _mkdir((dir + "\\" + app_name).c_str());
    if (retval != 0 && errno != EEXIST) return "";
  }
  return dir + "\\" + app_name;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
