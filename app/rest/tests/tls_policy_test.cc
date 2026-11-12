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

#include <cstdlib>
#include <map>
#include <string>

#include "app/rest/curl/curl_handles.h"
#include "app/src/include/firebase/internal/platform.h"
#include "curl/curl.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#if FIREBASE_PLATFORM_LINUX
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#endif  // FIREBASE_PLATFORM_LINUX

namespace firebase {
namespace rest {
namespace curl {
namespace {

class TlsPolicyTest : public ::testing::Test {
 protected:
  void SetUp() override { AcquireCurlGlobal(); }
  void TearDown() override { ReleaseCurlGlobal(); }
};

TEST_F(TlsPolicyTest, ApplyTlsPolicySucceedsOnFreshHandle) {
  UniqueEasy easy(curl_easy_init());
  ASSERT_NE(easy, nullptr);
  EXPECT_EQ(ApplyTlsPolicy(easy.get()), CURLE_OK);
  // Applying twice is harmless; every option is idempotent.
  EXPECT_EQ(ApplyTlsPolicy(easy.get()), CURLE_OK);
}

TEST_F(TlsPolicyTest, ApplyTlsPolicyReportsSetoptFailure) {
  // A null handle makes every curl_easy_setopt() fail, and the first failure
  // must be surfaced rather than swallowed.
  EXPECT_NE(ApplyTlsPolicy(nullptr), CURLE_OK);
}

TEST_F(TlsPolicyTest, CachedPathsAreStable) {
  // The cached accessors must return the same pointer on every call so libcurl
  // never observes a dangling or changing string.
  const char* bundle_first = LinuxCaBundlePath();
  const char* bundle_second = LinuxCaBundlePath();
  EXPECT_EQ(bundle_first, bundle_second);
  const char* dir_first = LinuxCaDirectoryPath();
  const char* dir_second = LinuxCaDirectoryPath();
  EXPECT_EQ(dir_first, dir_second);
#if !FIREBASE_PLATFORM_LINUX
  EXPECT_EQ(bundle_first, nullptr);
  EXPECT_EQ(dir_first, nullptr);
#endif  // !FIREBASE_PLATFORM_LINUX
}

#if FIREBASE_PLATFORM_LINUX

// Creates and removes temporary files/directories and saves and restores the
// environment variables the resolver reads, so each test starts clean.
class LinuxCaResolverTest : public ::testing::Test {
 protected:
  static constexpr const char* kEnvVars[] = {"CURL_CA_BUNDLE", "SSL_CERT_FILE",
                                             "SSL_CERT_DIR"};

  void SetUp() override {
    for (const char* name : kEnvVars) {
      const char* value = std::getenv(name);
      saved_[name] = value != nullptr ? std::string(value) : std::string();
      saved_present_[name] = value != nullptr;
      unsetenv(name);
    }
    char file_template[] = "/tmp/firebase_tls_policy_test_file_XXXXXX";
    int fd = mkstemp(file_template);
    ASSERT_GE(fd, 0);
    close(fd);
    temp_file_ = file_template;
    char dir_template[] = "/tmp/firebase_tls_policy_test_dir_XXXXXX";
    ASSERT_NE(mkdtemp(dir_template), nullptr);
    temp_dir_ = dir_template;
  }

  void TearDown() override {
    std::remove(temp_file_.c_str());
    rmdir(temp_dir_.c_str());
    for (const char* name : kEnvVars) {
      if (saved_present_[name]) {
        setenv(name, saved_[name].c_str(), 1);
      } else {
        unsetenv(name);
      }
    }
  }

  std::string temp_file_;
  std::string temp_dir_;
  std::map<std::string, std::string> saved_;
  std::map<std::string, bool> saved_present_;
};

constexpr const char* LinuxCaResolverTest::kEnvVars[];

TEST_F(LinuxCaResolverTest, CurlCaBundleEnvVarWins) {
  setenv("CURL_CA_BUNDLE", temp_file_.c_str(), 1);
  setenv("SSL_CERT_FILE", "/nonexistent/ssl_cert_file.pem", 1);
  EXPECT_EQ(internal::ResolveLinuxCaBundlePath(), temp_file_);
}

TEST_F(LinuxCaResolverTest, SslCertFileIsSecondChoice) {
  setenv("CURL_CA_BUNDLE", "/nonexistent/curl_ca_bundle.pem", 1);
  setenv("SSL_CERT_FILE", temp_file_.c_str(), 1);
  EXPECT_EQ(internal::ResolveLinuxCaBundlePath(), temp_file_);
}

TEST_F(LinuxCaResolverTest, UnreadableOrNonRegularEnvPathsAreSkipped) {
  // A directory is not a bundle file, and a missing path is not either; both
  // must fall through to the distribution defaults instead of being returned.
  setenv("CURL_CA_BUNDLE", temp_dir_.c_str(), 1);
  setenv("SSL_CERT_FILE", "/nonexistent/ssl_cert_file.pem", 1);
  std::string resolved = internal::ResolveLinuxCaBundlePath();
  EXPECT_NE(resolved, temp_dir_);
  EXPECT_NE(resolved, "/nonexistent/ssl_cert_file.pem");
  EXPECT_THAT(resolved, ::testing::AnyOf(::testing::IsEmpty(),
                                         ::testing::StartsWith("/etc/")));
}

TEST_F(LinuxCaResolverTest, DistributionDefaultIsAReadableFileOrEmpty) {
  std::string resolved = internal::ResolveLinuxCaBundlePath();
  if (!resolved.empty()) {
    struct stat st;
    ASSERT_EQ(stat(resolved.c_str(), &st), 0) << resolved;
    EXPECT_TRUE(S_ISREG(st.st_mode)) << resolved;
    EXPECT_EQ(access(resolved.c_str(), R_OK), 0) << resolved;
  }
}

TEST_F(LinuxCaResolverTest, SslCertDirEnvVarWins) {
  setenv("SSL_CERT_DIR", temp_dir_.c_str(), 1);
  EXPECT_EQ(internal::ResolveLinuxCaDirectoryPath(), temp_dir_);
}

TEST_F(LinuxCaResolverTest, SslCertDirMustBeADirectory) {
  setenv("SSL_CERT_DIR", temp_file_.c_str(), 1);
  std::string resolved = internal::ResolveLinuxCaDirectoryPath();
  EXPECT_NE(resolved, temp_file_);
  EXPECT_THAT(resolved, ::testing::AnyOf(::testing::IsEmpty(),
                                         ::testing::Eq("/etc/ssl/certs")));
}

TEST_F(LinuxCaResolverTest, NullCaInfoAndCaPathAreAcceptedByLibcurl) {
  // When no bundle/directory is found, ApplyTlsPolicy() still sets
  // CURLOPT_CAINFO / CURLOPT_CAPATH with a null value so libcurl does not fall
  // back to the build host's compiled-in CURL_CA_BUNDLE. This pins the libcurl
  // contract that a null value is accepted rather than reported as an error
  // (the cached LinuxCaBundlePath() cannot be forced to null on a host that
  // has a system bundle, so test the setopt calls directly).
  AcquireCurlGlobal();
  {
    UniqueEasy easy(curl_easy_init());
    ASSERT_NE(easy, nullptr);
    CURLcode first_error = CURLE_OK;
    ApplyOpt(easy.get(), CURLOPT_CAINFO, static_cast<const char*>(nullptr),
             &first_error);
    EXPECT_EQ(first_error, CURLE_OK);
    CURLcode capath_code = curl_easy_setopt(easy.get(), CURLOPT_CAPATH,
                                            static_cast<const char*>(nullptr));
    EXPECT_TRUE(capath_code == CURLE_OK || capath_code == CURLE_NOT_BUILT_IN)
        << capath_code;
  }
  ReleaseCurlGlobal();
}

#endif  // FIREBASE_PLATFORM_LINUX

}  // namespace
}  // namespace curl
}  // namespace rest
}  // namespace firebase
