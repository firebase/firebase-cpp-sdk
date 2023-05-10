// Copyright 2019 Google Inc. All rights reserved.
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

#include "firebase_test_framework.h"  // NOLINT

namespace firebase_test_framework {

using app_framework::LogWarning;

bool FirebaseTest::SendHttpGetRequest(
    const char* url, const std::map<std::string, std::string>& headers,
    int* response_code, std::string* response_str) {
  LogWarning("SendHttpGetRequest is not implemented on desktop.");
  return false;
}

bool FirebaseTest::SendHttpPostRequest(
    const char* url, const std::map<std::string, std::string>& headers,
    const std::string& post_body, int* response_code,
    std::string* response_str) {
  LogWarning("SendHttpPostRequest is not implemented on desktop.");
  return false;
}

bool FirebaseTest::OpenUrlInBrowser(const char* url) {
  LogWarning("OpenUrlInBrowser is not implemented on desktop.");
  return false;
}

bool FirebaseTest::SetPersistentString(const char* key, const char* value) {
  LogWarning("SetPersistentString is not implemented on desktop.");
  return false;
}

bool FirebaseTest::GetPersistentString(const char* key,
                                       std::string* value_out) {
  LogWarning("GetPersistentString is not implemented on desktop.");
  return false;
}

bool FirebaseTest::IsRunningOnEmulator() {
  // No emulators on desktop.
  return false;
}

int FirebaseTest::GetGooglePlayServicesVersion() {
  // No Google Play services on desktop.
  return 0;
}

}  // namespace firebase_test_framework
