// Copyright 2024 Google LLC
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

#include "app_check/src/desktop/debug_provider_desktop.h"

#include <sys/stat.h>

#include <chrono>  // NOLINT
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <future>  // NOLINT
#include <memory>
#include <mutex>  // NOLINT
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "app/rest/request.h"
#include "app/rest/response.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_interface.h"
#include "app/rest/transport_mock.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "app/src/uuid.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "firebase/app_check.h"
#include "firebase/app_check/debug_provider.h"
#include "flatbuffers/stl_emulation.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"

namespace firebase {
namespace app_check {
namespace internal {

namespace {

const char kTestProjectId[] = "test-project-id";
const char kTestAppId[] = "1:1234567890:android:abcdef123456";
const char kTestApiKey[] = "AIzaSyTestApiKey1234567890";
const char kExpectedExchangeUrl[] =
    "https://firebaseappcheck.googleapis.com/v1/projects/test-project-id/apps/"
    "1:1234567890:android:abcdef123456:exchangeDebugToken";

struct CapturedLogEntry {
  LogLevel level;
  std::string message;
};

std::mutex g_log_capture_mutex;
std::vector<CapturedLogEntry>* g_captured_logs = nullptr;

std::mutex g_request_capture_mutex;
std::string* g_last_request_body = nullptr;
std::string* g_last_request_url = nullptr;

void TestLogCallback(LogLevel log_level, const char* log_message,
                     void* /*callback_data*/) {
  std::lock_guard<std::mutex> lock(g_log_capture_mutex);
  if (g_captured_logs && log_message) {
    g_captured_logs->push_back({log_level, std::string(log_message)});
  }
}

class CapturingTransportMock : public rest::TransportMock {
 public:
  void PerformInternal(
      rest::Request* request, rest::Response* response,
      flatbuffers::unique_ptr<rest::Controller>* controller_out) override {
    if (request) {
      std::string body;
      request->ReadBodyIntoString(&body);
      std::lock_guard<std::mutex> lock(g_request_capture_mutex);
      if (g_last_request_body) {
        *g_last_request_body = body;
      }
      if (g_last_request_url) {
        *g_last_request_url = request->options().url;
      }
    }
    rest::TransportMock::PerformInternal(request, response, controller_out);
  }
};

void SetEnvVar(const char* name, const char* value) {
#if defined(_WIN32)
  _putenv_s(name, value ? value : "");
#else
  if (value) {
    setenv(name, value, 1);
  } else {
    unsetenv(name);
  }
#endif
}

std::pair<bool, std::string> GetEnvVar(const char* name) {
  const char* val = std::getenv(name);
  if (!val) {
    return {false, ""};
  }
  return {true, std::string(val)};
}

void RestoreEnvVar(const char* name, const std::pair<bool, std::string>& orig) {
  if (orig.first) {
    SetEnvVar(name, orig.second.c_str());
  } else {
    SetEnvVar(name, nullptr);
  }
}

bool MakeDir(const std::string& path) {
#if defined(_WIN32)
  return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
  return mkdir(path.c_str(), 0700) == 0 || errno == EEXIST;
#endif
}

void RemoveDir(const std::string& path) {
#if defined(_WIN32)
  _rmdir(path.c_str());
#else
  rmdir(path.c_str());
#endif
}

bool IsValidUuidFormat(const std::string& str) {
  if (str.length() != 36) {
    return false;
  }
  for (size_t i = 0; i < str.length(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (str[i] != '-') return false;
    } else {
      char c = str[i];
      bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F');
      if (!is_hex) return false;
    }
  }
  return true;
}

class DebugProviderDesktopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    orig_firebase_debug_token_ = GetEnvVar("FIREBASE_APPCHECK_DEBUG_TOKEN");
    orig_legacy_debug_token_ = GetEnvVar("AppCheckDebugToken");
    orig_old_cpp_debug_token_ = GetEnvVar("APP_CHECK_DEBUG_TOKEN");
    orig_xdg_config_home_ = GetEnvVar("XDG_CONFIG_HOME");
    orig_home_ = GetEnvVar("HOME");
    orig_userprofile_ = GetEnvVar("USERPROFILE");

    SetEnvVar("FIREBASE_APPCHECK_DEBUG_TOKEN", nullptr);
    SetEnvVar("AppCheckDebugToken", nullptr);
    SetEnvVar("APP_CHECK_DEBUG_TOKEN", nullptr);
    SetEnvVar("XDG_CONFIG_HOME", nullptr);

    // Create an isolated temporary directory for this test.
    firebase::internal::Uuid uuid;
    uuid.Generate();
    char suffix[37];
    snprintf(
        suffix, sizeof(suffix),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid.data[0], uuid.data[1], uuid.data[2], uuid.data[3], uuid.data[4],
        uuid.data[5], uuid.data[6], uuid.data[7], uuid.data[8], uuid.data[9],
        uuid.data[10], uuid.data[11], uuid.data[12], uuid.data[13],
        uuid.data[14], uuid.data[15]);
#if defined(_WIN32)
    const char* tmp_base = std::getenv("TEMP");
    if (!tmp_base) tmp_base = ".";
#else
    const char* tmp_base = "/tmp";
#endif
    temp_dir_ = std::string(tmp_base) + "/firebase_appcheck_test_" + suffix;
    MakeDir(temp_dir_);
    SetEnvVar("HOME", temp_dir_.c_str());
    SetEnvVar("USERPROFILE", temp_dir_.c_str());

    // Default to non-Release simulation so configstore tests run consistently
    // even if CMake built tests with -DCMAKE_BUILD_TYPE=Release.
    SetSimulateReleaseBuildForTesting(false);
    DebugAppCheckProviderFactory::GetInstance()->SetDebugToken("");

    {
      std::lock_guard<std::mutex> lock(g_log_capture_mutex);
      delete g_captured_logs;
      g_captured_logs = new std::vector<CapturedLogEntry>();
    }
    {
      std::lock_guard<std::mutex> lock(g_request_capture_mutex);
      delete g_last_request_body;
      g_last_request_body = new std::string();
      delete g_last_request_url;
      g_last_request_url = new std::string();
    }

    orig_log_level_ = GetLogLevel();
    SetLogLevel(kLogLevelVerbose);
    LogSetCallback(TestLogCallback, nullptr);

    rest::SetTransportBuilder([]() -> flatbuffers::unique_ptr<rest::Transport> {
      return flatbuffers::unique_ptr<rest::Transport>(
          new CapturingTransportMock());
    });

    AppOptions options = testing::MockAppOptions();
    options.set_project_id(kTestProjectId);
    options.set_app_id(kTestAppId);
    options.set_api_key(kTestApiKey);
    app_ = testing::CreateApp(options);
  }

  void TearDown() override {
    LogSetCallback(nullptr, nullptr);
    SetLogLevel(orig_log_level_);
    rest::SetTransportBuilder(nullptr);
    ResetSimulateReleaseBuildForTesting();
    DebugAppCheckProviderFactory::GetInstance()->SetDebugToken("");

    delete app_;
    app_ = nullptr;

    CleanupTempFiles();

    RestoreEnvVar("FIREBASE_APPCHECK_DEBUG_TOKEN", orig_firebase_debug_token_);
    RestoreEnvVar("AppCheckDebugToken", orig_legacy_debug_token_);
    RestoreEnvVar("APP_CHECK_DEBUG_TOKEN", orig_old_cpp_debug_token_);
    RestoreEnvVar("XDG_CONFIG_HOME", orig_xdg_config_home_);
    RestoreEnvVar("HOME", orig_home_);
    RestoreEnvVar("USERPROFILE", orig_userprofile_);

    {
      std::lock_guard<std::mutex> lock(g_log_capture_mutex);
      delete g_captured_logs;
      g_captured_logs = nullptr;
    }
    {
      std::lock_guard<std::mutex> lock(g_request_capture_mutex);
      delete g_last_request_body;
      g_last_request_body = nullptr;
      delete g_last_request_url;
      g_last_request_url = nullptr;
    }
  }

  std::string WriteHomeConfigstoreFile(const std::string& content) {
    std::string dot_config = temp_dir_ + "/.config";
    std::string configstore_dir = dot_config + "/configstore";
    MakeDir(dot_config);
    MakeDir(configstore_dir);
    std::string file_path = configstore_dir + "/firebase-appcheck.json";
    std::ofstream out(file_path.c_str(), std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    created_files_.push_back(file_path);
    created_dirs_.push_back(configstore_dir);
    created_dirs_.push_back(dot_config);
    return file_path;
  }

  std::string WriteXdgConfigstoreFile(const std::string& content) {
    std::string xdg_dir = temp_dir_ + "/xdg_custom";
    std::string configstore_dir = xdg_dir + "/configstore";
    MakeDir(xdg_dir);
    MakeDir(configstore_dir);
    std::string file_path = configstore_dir + "/firebase-appcheck.json";
    std::ofstream out(file_path.c_str(), std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    created_files_.push_back(file_path);
    created_dirs_.push_back(configstore_dir);
    created_dirs_.push_back(xdg_dir);
    SetEnvVar("XDG_CONFIG_HOME", xdg_dir.c_str());
    return file_path;
  }

  void CleanupTempFiles() {
    for (const std::string& f : created_files_) {
      std::remove(f.c_str());
    }
    created_files_.clear();
    for (const std::string& d : created_dirs_) {
      RemoveDir(d);
    }
    created_dirs_.clear();
    RemoveDir(temp_dir_);
  }

  bool HasLogMessage(LogLevel level, const std::string& substring) {
    std::lock_guard<std::mutex> lock(g_log_capture_mutex);
    if (!g_captured_logs) return false;
    for (const auto& entry : *g_captured_logs) {
      if (entry.level == level &&
          entry.message.find(substring) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  std::string GetLastRequestBody() {
    std::lock_guard<std::mutex> lock(g_request_capture_mutex);
    return g_last_request_body ? *g_last_request_body : "";
  }

  void ConfigureMockHttp200(const std::string& jwt_token = "mock-jwt-token",
                            const std::string& ttl = "3600s") {
    char config[1024];
    snprintf(config, sizeof(config),
             "{"
             "  config:["
             "    {fake:'%s',"
             "     httpresponse: {"
             "       header: ['HTTP/1.1 200 OK','Server:mock server 101'],"
             "       body: ['{\"token\":\"%s\",\"ttl\":\"%s\"}']"
             "     }"
             "    }"
             "  ]"
             "}",
             kExpectedExchangeUrl, jwt_token.c_str(), ttl.c_str());
    firebase::testing::cppsdk::ConfigSet(config);
  }

  void ConfigureMockHttpError(int status_code, const char* status_text,
                              const char* error_body) {
    char config[1024];
    snprintf(config, sizeof(config),
             "{"
             "  config:["
             "    {fake:'%s',"
             "     httpresponse: {"
             "       header: ['HTTP/1.1 %d %s','Server:mock server 101'],"
             "       body: ['%s']"
             "     }"
             "    }"
             "  ]"
             "}",
             kExpectedExchangeUrl, status_code, status_text, error_body);
    firebase::testing::cppsdk::ConfigSet(config);
  }

  App* app_ = nullptr;
  std::string temp_dir_;
  std::vector<std::string> created_files_;
  std::vector<std::string> created_dirs_;
  LogLevel orig_log_level_;

  std::pair<bool, std::string> orig_firebase_debug_token_;
  std::pair<bool, std::string> orig_legacy_debug_token_;
  std::pair<bool, std::string> orig_old_cpp_debug_token_;
  std::pair<bool, std::string> orig_xdg_config_home_;
  std::pair<bool, std::string> orig_home_;
  std::pair<bool, std::string> orig_userprofile_;
};

TEST_F(DebugProviderDesktopTest,
       SetDebugTokenTakesPrecedenceOverEnvAndConfigstore) {
  SetEnvVar("FIREBASE_APPCHECK_DEBUG_TOKEN", "env-token-primary");
  WriteHomeConfigstoreFile(std::string("{\"tokens\":{\"") + kTestAppId +
                           "\":\"configstore-token\"}}");
  DebugAppCheckProviderFactory::GetInstance()->SetDebugToken(
      "explicit-factory-token");

  ConfigureMockHttp200("minted-jwt-explicit");

  AppCheckProvider* provider =
      DebugAppCheckProviderFactory::GetInstance()->CreateProvider(app_);
  ASSERT_NE(provider, nullptr);

  std::promise<void> done;
  AppCheckToken received_token;
  int received_error = -1;
  provider->GetToken([&](AppCheckToken token, int error_code,
                         const std::string& /*error_message*/) {
    received_token = token;
    received_error = error_code;
    done.set_value();
  });

  ASSERT_EQ(done.get_future().wait_for(std::chrono::seconds(5)),
            std::future_status::ready);
  EXPECT_EQ(received_error, kAppCheckErrorNone);
  EXPECT_EQ(received_token.token, "minted-jwt-explicit");
  EXPECT_THAT(GetLastRequestBody(),
              ::testing::HasSubstr("explicit-factory-token"));
  EXPECT_TRUE(HasLogMessage(
      kLogLevelInfo,
      "[FirebaseAppCheck] Debug token loaded from SetDebugToken."));
}

TEST_F(DebugProviderDesktopTest,
       ResolvesFromFirebaseAppCheckDebugTokenEnvAndTakesPrecedence) {
  SetEnvVar("FIREBASE_APPCHECK_DEBUG_TOKEN", "env-token-primary");
  SetEnvVar("AppCheckDebugToken", "env-token-legacy");
  WriteHomeConfigstoreFile(std::string("{\"tokens\":{\"") + kTestAppId +
                           "\":\"configstore-token\"}}");

  ConfigureMockHttp200("minted-appcheck-jwt");

  AppCheckProvider* provider =
      DebugAppCheckProviderFactory::GetInstance()->CreateProvider(app_);
  ASSERT_NE(provider, nullptr);

  std::promise<void> done;
  AppCheckToken received_token;
  int received_error = -1;
  provider->GetToken([&](AppCheckToken token, int error_code,
                         const std::string& /*error_message*/) {
    received_token = token;
    received_error = error_code;
    done.set_value();
  });

  ASSERT_EQ(done.get_future().wait_for(std::chrono::seconds(5)),
            std::future_status::ready);
  EXPECT_EQ(received_error, kAppCheckErrorNone);
  EXPECT_EQ(received_token.token, "minted-appcheck-jwt");
  EXPECT_THAT(GetLastRequestBody(), ::testing::HasSubstr("env-token-primary"));
  EXPECT_TRUE(HasLogMessage(
      kLogLevelInfo,
      "[FirebaseAppCheck] Debug token loaded from environment variable."));
}

TEST_F(DebugProviderDesktopTest, ResolvesFromLegacyAppCheckDebugTokenEnvVar) {
  SetEnvVar("AppCheckDebugToken", "env-token-legacy-fallback");
  WriteHomeConfigstoreFile(std::string("{\"tokens\":{\"") + kTestAppId +
                           "\":\"configstore-token\"}}");

  DebugAppCheckProvider provider(app_, "");
  EXPECT_EQ(provider.DetermineDebugToken(), "env-token-legacy-fallback");
  EXPECT_TRUE(HasLogMessage(
      kLogLevelInfo,
      "[FirebaseAppCheck] Debug token loaded from environment variable."));
}

TEST_F(DebugProviderDesktopTest, ResolvesFromConfigstoreInHomeDirectory) {
  WriteHomeConfigstoreFile(
      std::string("{\n"
                  "  \"_comment\": \"This file contains secrets\",\n"
                  "  \"tokens\": {\n"
                  "    \"1:999:ios:other\": \"other-token\",\n"
                  "    \"") +
      kTestAppId +
      "\": \"configstore-secret-token-123\"\n"
      "  }\n"
      "}");

  ConfigureMockHttp200("jwt-from-configstore");

  AppCheckProvider* provider =
      DebugAppCheckProviderFactory::GetInstance()->CreateProvider(app_);
  ASSERT_NE(provider, nullptr);

  std::promise<void> done;
  AppCheckToken received_token;
  int received_error = -1;
  provider->GetToken([&](AppCheckToken token, int error_code,
                         const std::string& /*error_message*/) {
    received_token = token;
    received_error = error_code;
    done.set_value();
  });

  ASSERT_EQ(done.get_future().wait_for(std::chrono::seconds(5)),
            std::future_status::ready);
  EXPECT_EQ(received_error, kAppCheckErrorNone);
  EXPECT_EQ(received_token.token, "jwt-from-configstore");
  EXPECT_THAT(GetLastRequestBody(),
              ::testing::HasSubstr("configstore-secret-token-123"));
  EXPECT_TRUE(HasLogMessage(kLogLevelInfo,
                            "[FirebaseAppCheck] Debug token loaded from "
                            "~/.config/configstore/firebase-appcheck.json."));
}

TEST_F(DebugProviderDesktopTest, ResolvesFromConfigstoreInXdgConfigHome) {
  WriteHomeConfigstoreFile(std::string("{\"tokens\":{\"") + kTestAppId +
                           "\":\"home-token\"}}");
  WriteXdgConfigstoreFile(std::string("{\"tokens\":{\"") + kTestAppId +
                          "\":\"xdg-token-456\"}}");

  DebugAppCheckProvider provider(app_, "");
  EXPECT_EQ(provider.DetermineDebugToken(), "xdg-token-456");
  EXPECT_TRUE(HasLogMessage(kLogLevelInfo,
                            "[FirebaseAppCheck] Debug token loaded from "
                            "~/.config/configstore/firebase-appcheck.json."));
}

TEST_F(DebugProviderDesktopTest,
       FallbackToUuidWhenAppIdNotPresentInConfigstore) {
  WriteHomeConfigstoreFile(
      "{\"tokens\":{\"1:000000:web:different_app\":\"other-app-token\"}}");

  DebugAppCheckProvider provider(app_, "");
  std::string resolved = provider.DetermineDebugToken();
  EXPECT_TRUE(IsValidUuidFormat(resolved)) << "Resolved token: " << resolved;
  EXPECT_TRUE(HasLogMessage(
      kLogLevelInfo,
      std::string("contains no debug token for app ID '") + kTestAppId + "'"));
  EXPECT_TRUE(HasLogMessage(
      kLogLevelInfo,
      "[FirebaseAppCheck] Debug token loaded from auto-generated UUID."));
  EXPECT_TRUE(
      HasLogMessage(kLogLevelWarning,
                    "WARNING: Firebase App Check debug token: " + resolved));
  // Verify the final part of the warning was not truncated by the 511-char
  // log buffer in app/src/log.cc.
  EXPECT_TRUE(HasLogMessage(
      kLogLevelWarning,
      "Warning: This debug token is a secret and should not be shared"));
}

TEST_F(DebugProviderDesktopTest,
       FallbackToUuidWhenConfigstoreFileDoesNotExist) {
  DebugAppCheckProvider provider(app_, "");
  std::string resolved = provider.DetermineDebugToken();
  EXPECT_TRUE(IsValidUuidFormat(resolved)) << "Resolved token: " << resolved;
  EXPECT_FALSE(HasLogMessage(kLogLevelWarning, "Failed to open"));
  EXPECT_FALSE(HasLogMessage(kLogLevelWarning, "Failed to parse"));
  EXPECT_TRUE(HasLogMessage(
      kLogLevelInfo,
      "[FirebaseAppCheck] Debug token loaded from auto-generated UUID."));
}

TEST_F(DebugProviderDesktopTest,
       FallbackToUuidWhenConfigstoreJsonIsMalformedOrInvalid) {
  // 1. Malformed JSON syntax
  std::string path = WriteHomeConfigstoreFile("{ invalid json: [");
  EXPECT_EQ(ResolveDebugTokenFromConfig(path, kTestAppId), "");
  EXPECT_TRUE(HasLogMessage(kLogLevelWarning, "Failed to parse JSON in"));

  DebugAppCheckProvider provider_malformed(app_, "");
  EXPECT_TRUE(IsValidUuidFormat(provider_malformed.DetermineDebugToken()));

  // 2. Empty file
  path = WriteHomeConfigstoreFile("");
  EXPECT_EQ(ResolveDebugTokenFromConfig(path, kTestAppId), "");

  // 3. JSON array instead of object
  path = WriteHomeConfigstoreFile("[\"not\", \"an\", \"object\"]");
  EXPECT_EQ(ResolveDebugTokenFromConfig(path, kTestAppId), "");

  // 4. "tokens" field is not a map
  path = WriteHomeConfigstoreFile("{\"tokens\": \"not-a-map\"}");
  EXPECT_EQ(ResolveDebugTokenFromConfig(path, kTestAppId), "");

  // 5. Token value for app_id is not a string
  path = WriteHomeConfigstoreFile(std::string("{\"tokens\":{\"") + kTestAppId +
                                  "\": 12345}}");
  EXPECT_EQ(ResolveDebugTokenFromConfig(path, kTestAppId), "");
}

TEST_F(DebugProviderDesktopTest,
       ReleaseBuildLogsWarningAndStillResolvesConfigstoreToken) {
  WriteHomeConfigstoreFile(std::string("{\"tokens\":{\"") + kTestAppId +
                           "\":\"secret-configstore-token\"}}");
  SetSimulateReleaseBuildForTesting(true);

  DebugAppCheckProvider provider(app_, "");
  std::string resolved = provider.DetermineDebugToken();
  EXPECT_EQ(resolved, "secret-configstore-token");
  EXPECT_TRUE(HasLogMessage(
      kLogLevelWarning,
      "[FirebaseAppCheck] DebugAppCheckProviderFactory is in use in a Release "
      "build (NDEBUG defined). Debug tokens are for development only; prefer "
      "a production provider where one is available."));
  EXPECT_TRUE(HasLogMessage(kLogLevelInfo,
                            "[FirebaseAppCheck] Debug token loaded from "
                            "~/.config/configstore/firebase-appcheck.json."));
}

TEST_F(DebugProviderDesktopTest,
       Http403ResponseLogsActionableRecoveryWarningAndFailsCallback) {
  SetEnvVar("FIREBASE_APPCHECK_DEBUG_TOKEN", "revoked-debug-token");
  ConfigureMockHttpError(
      403, "Forbidden",
      "{\"error\":{\"code\":403,\"message\":\"App attestation failed.\"}}");

  AppCheckProvider* provider =
      DebugAppCheckProviderFactory::GetInstance()->CreateProvider(app_);
  ASSERT_NE(provider, nullptr);

  std::promise<void> done;
  int received_error = kAppCheckErrorNone;
  std::string received_error_message;
  provider->GetToken([&](AppCheckToken /*token*/, int error_code,
                         const std::string& error_message) {
    received_error = error_code;
    received_error_message = error_message;
    done.set_value();
  });

  ASSERT_EQ(done.get_future().wait_for(std::chrono::seconds(5)),
            std::future_status::ready);
  EXPECT_EQ(received_error, kAppCheckErrorUnknown);
  EXPECT_THAT(received_error_message, ::testing::HasSubstr("403"));
  EXPECT_TRUE(HasLogMessage(
      kLogLevelWarning,
      "[FirebaseAppCheck] Debug token was rejected by server. Run 'firebase "
      "appcheck:debugtokens:create --force' to provision a new token."));
}

}  // namespace
}  // namespace internal
}  // namespace app_check
}  // namespace firebase
