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

#include "app/src/log.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace firebase {
namespace app {
namespace secure {

static const char kTestKeychainPassword[] = "password";

UserSecureDarwinTestHelper::UserSecureDarwinTestHelper() {
  SecKeychainRef keychain;
  char temp_file[] = "test_keychain_XXXXXX";
  std::string filename = mktemp(temp_file);
  if (filename.empty()) {
    LogError("UserSecureDarwinTestHelper: Couldn't create temporary file");
    return;
  }
  OSStatus status = SecKeychainCreate(filename.c_str(), strlen(kTestKeychainPassword),
                                      kTestKeychainPassword, FALSE, NULL, &keychain);
  if (status != noErr) {
    NSString* error_string = (__bridge_transfer NSString*)SecCopyErrorMessageString(status, NULL);
    LogError("UserSecureDarwinTestHelper: Error %d creating %s: %s", status, filename.c_str(),
             error_string.UTF8String);
    return;
  }
  status = SecKeychainSetDefault(keychain);
  CFRelease(keychain);
  if (status != noErr) {
    NSString* error_string = (__bridge_transfer NSString*)SecCopyErrorMessageString(status, NULL);
    LogError("UserSecureDarwinTestHelper: Error %d setting default keychain to %s: %s", status,
             filename.c_str(), error_string.UTF8String);
    return;
  }
  LogInfo("UserSecureDarwinTestHelper: Using test keychain: %s", filename.c_str());
  test_keychain_file_ = filename;
  Boolean prev_interaction = YES;
  SecKeychainGetUserInteractionAllowed(&prev_interaction);
  previous_interaction_mode_ = (prev_interaction != NO);
  SecKeychainSetUserInteractionAllowed(NO);
}

UserSecureDarwinTestHelper::~UserSecureDarwinTestHelper() {
  if (!test_keychain_file_.empty()) {
    unlink(test_keychain_file_.c_str());
    test_keychain_file_ = "";
    SecKeychainSetUserInteractionAllowed(previous_interaction_mode_ ? YES : NO);
  }
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
