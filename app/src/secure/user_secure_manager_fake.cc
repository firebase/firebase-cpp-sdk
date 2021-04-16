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

#include "app/src/secure/user_secure_manager_fake.h"

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/secure/user_secure_fake_internal.h"

namespace firebase {
namespace app {
namespace secure {

#if FIREBASE_PLATFORM_WINDOWS
static const char kDirectorySeparator[] = "\\";
#else
static const char kDirectorySeparator[] = "/";
#endif  // FIREBASE_PLATFORM_WINDOWS

// Get temp directory for fake secure storage.
static std::string GetTestTmpDir(const char test_namespace[]) {
#if FIREBASE_PLATFORM_WINDOWS
  char buf[MAX_PATH + 1];
  if (GetEnvironmentVariable("TEST_TMPDIR", buf, sizeof(buf))) {
    return std::string(buf) + kDirectorySeparator + test_namespace;
  }
#else
  // Linux and OS X should either have the TEST_TMPDIR environment variable set.
  if (const char* value = getenv("TEST_TMPDIR")) {
    return std::string(value) + kDirectorySeparator + test_namespace;
  }
#endif  // FIREBASE_PLATFORM_WINDOWS
  // If we weren't able to get TEST_TMPDIR, just use a subdirectory.
  return test_namespace;
}

UserSecureManagerFake::UserSecureManagerFake(const char* domain,
                                             const char* app_id)
    : UserSecureManager(MakeUnique<UserSecureFakeInternal>(
          domain, GetTestTmpDir(app_id).c_str())) {}

}  // namespace secure
}  // namespace app
}  // namespace firebase
