// Copyright 2019 Google LLC
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

#include "app/src/secure/user_secure_darwin_internal_testlib.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#include <unistd.h>

#include <cstring>
#include <map>
#include <mutex>
#include <string>

#include "app/src/log.h"

namespace firebase {
namespace app {
namespace secure {

namespace {

constexpr char kTestKeychainPassword[] = "password";

std::mutex& GetPreviousKeychainsMutex() {
  static auto* mutex = new std::mutex();
  return *mutex;
}

std::map<const UserSecureDarwinTestHelper*, SecKeychainRef>&
GetPreviousKeychains() {
  static auto* map =
      new std::map<const UserSecureDarwinTestHelper*, SecKeychainRef>();
  return *map;
}

std::string GetSecurityErrorMessage(OSStatus status) {
  CFStringRef cf_error = SecCopyErrorMessageString(status, nullptr);
  if (cf_error == nullptr) {
    return "Unknown error";
  }
  const char* utf8 = [(__bridge NSString*)cf_error UTF8String];
  std::string message = utf8 ? utf8 : "Unknown error";
  CFRelease(cf_error);
  return message;
}

}  // namespace

UserSecureDarwinTestHelper::UserSecureDarwinTestHelper()
    : previous_interaction_mode_(true) {
  SecKeychainRef keychain = nullptr;
  char temp_file[] = "/tmp/test_keychain_XXXXXX";
  int fd = mkstemp(temp_file);
  if (fd == -1) {
    LogError("UserSecureDarwinTestHelper: Couldn't create temporary file");
    return;
  }
  close(fd);
  unlink(temp_file);
  std::string filename = temp_file;
  OSStatus status = SecKeychainCreate(
      filename.c_str(), static_cast<UInt32>( std::strlen(kTestKeychainPassword)),
      kTestKeychainPassword, FALSE, nullptr, &keychain);
  if (status != noErr) {
    std::string error_message = GetSecurityErrorMessage(status);
    LogError("UserSecureDarwinTestHelper: Error %d creating %s: %s",
             static_cast<int>(status), filename.c_str(), error_message.c_str());
    return;
  }

  SecKeychainRef previous_keychain = nullptr;
  if (SecKeychainCopyDefault(&previous_keychain) != noErr) {
    previous_keychain = nullptr;
  }

  status = SecKeychainSetDefault(keychain);
  if (status != noErr) {
    std::string error_message = GetSecurityErrorMessage(status);
    LogError(
        "UserSecureDarwinTestHelper: Error %d setting default keychain to %s: "
        "%s",
        static_cast<int>(status), filename.c_str(), error_message.c_str());
    SecKeychainDelete(keychain);
    CFRelease(keychain);
    unlink(filename.c_str());
    if (previous_keychain != nullptr) {
      CFRelease(previous_keychain);
    }
    return;
  }
  CFRelease(keychain);

  if (previous_keychain != nullptr) {
    std::lock_guard<std::mutex> lock(GetPreviousKeychainsMutex());
    GetPreviousKeychains()[this] = previous_keychain;
  }

  LogInfo("UserSecureDarwinTestHelper: Using test keychain: %s",
          filename.c_str());
  test_keychain_file_ = filename;
  Boolean prev_interaction = YES;
  SecKeychainGetUserInteractionAllowed(&prev_interaction);
  previous_interaction_mode_ = (prev_interaction != NO);
  SecKeychainSetUserInteractionAllowed(NO);
}

UserSecureDarwinTestHelper::~UserSecureDarwinTestHelper() {
  if (!test_keychain_file_.empty()) {
    SecKeychainRef keychain = nullptr;
    if (SecKeychainOpen(test_keychain_file_.c_str(), &keychain) == noErr &&
        keychain != nullptr) {
      SecKeychainDelete(keychain);
      CFRelease(keychain);
    }
    unlink(test_keychain_file_.c_str());
    test_keychain_file_ = "";

    SecKeychainRef previous_keychain = nullptr;
    {
      std::lock_guard<std::mutex> lock(GetPreviousKeychainsMutex());
      auto& map = GetPreviousKeychains();
      auto it = map.find(this);
      if (it != map.end()) {
        previous_keychain = it->second;
        map.erase(it);
      }
    }
    if (previous_keychain != nullptr) {
      SecKeychainSetDefault(previous_keychain);
      CFRelease(previous_keychain);
    }

    SecKeychainSetUserInteractionAllowed(previous_interaction_mode_ ? YES : NO);
  }
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
