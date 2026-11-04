/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/rest/curl/tls_policy.h"

#include <cstring>

#include "app/rest/curl/curl_io_loop.h"
#include "curl/curl.h"
#include "gtest/gtest.h"

#if defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace firebase {
namespace rest {
namespace curl {
namespace {

class TlsPolicyTest : public ::testing::Test {
 protected:
  void SetUp() override { (void)curl_global_init(CURL_GLOBAL_ALL); }
  void TearDown() override { curl_global_cleanup(); }
};

TEST_F(TlsPolicyTest, ApplyTlsPolicySucceedsAndSurvivesEasyReset) {
  UniqueEasy easy(curl_easy_init());
  ASSERT_NE(nullptr, easy.get());
  EXPECT_EQ(CURLE_OK, ApplyTlsPolicy(easy.get()));

  curl_easy_reset(easy.get());
  EXPECT_EQ(CURLE_OK, ApplyTlsPolicy(easy.get()));
}

TEST_F(TlsPolicyTest, UniqueHandlesAreSafeOnNullAndSListChaining) {
  UniqueEasy null_easy(nullptr);
  UniqueMulti null_multi(nullptr);
  UniqueSList headers(nullptr);

  headers.reset(curl_slist_append(headers.release(), "X-First: 1"));
  headers.reset(curl_slist_append(headers.release(), "X-Second: 2"));
  ASSERT_NE(nullptr, headers.get());
  EXPECT_STREQ("X-First: 1", headers->data);
  ASSERT_NE(nullptr, headers->next);
  EXPECT_STREQ("X-Second: 2", headers->next->data);
  EXPECT_EQ(nullptr, headers->next->next);
}

#if defined(__linux__)
TEST_F(TlsPolicyTest, LinuxRuntimeCaPathsAreReadableWhenPresent) {
  const char* bundle = LinuxCaBundlePath();
  if (bundle != nullptr) {
    struct stat st;
    ASSERT_EQ(0, stat(bundle, &st)) << bundle;
    EXPECT_TRUE(S_ISREG(st.st_mode)) << bundle;
    EXPECT_EQ(0, access(bundle, R_OK)) << bundle;
  }

  const char* capath = LinuxCaDirectoryPath();
  if (capath != nullptr) {
    struct stat st;
    ASSERT_EQ(0, stat(capath, &st)) << capath;
    EXPECT_TRUE(S_ISDIR(st.st_mode)) << capath;
    EXPECT_EQ(0, access(capath, R_OK | X_OK)) << capath;
  }

  // On any standard Linux CI/developer machine at least one of the two trust
  // store locations is populated.
  EXPECT_TRUE(bundle != nullptr || capath != nullptr);
}
#endif  // defined(__linux__)

}  // namespace
}  // namespace curl
}  // namespace rest
}  // namespace firebase
