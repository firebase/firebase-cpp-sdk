// Copyright 2026 Google LLC
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

#include <chrono>  // NOLINT
#include <cstdlib>
#include <filesystem>  // NOLINT
#include <fstream>
#include <future>  // NOLINT
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "app/rest/transport_builder.h"
#include "app/rest/transport_interface.h"
#include "app/rest/transport_mock.h"
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "firebase/app_check.h"
#include "firebase/app_check/debug_provider.h"
#include "flatbuffers/stl_emulation.h"
#include "gtest/gtest.h"
#include "testing/config.h"

namespace firebase {
namespace app_check {
namespace internal {
namespace {

namespace fs = std::filesystem;

constexpr char kTestProjectId[] = "test-project-id";
constexpr char kTestAppId[] = "1:1234567890:android:abcdef123456";
constexpr char kTestApiKey[] = "AIzaSyTestApiKey1234567890";
constexpr char kExpectedExchangeUrl[] =
    "https://firebaseappcheck.googleapis.com/v1/projects/test-project-id/apps/"
    "1:1234567890:android:abcdef123456:exchangeDebugToken";

constexpr const char* kTrackedEnvVars[] = {
    "FIREBASE_APPCHECK_DEBUG_TOKEN",
    "AppCheckDebugToken",
    "APP_CHECK_DEBUG_TOKEN",
    "XDG_CONFIG_HOME",
    "HOME",
    "USERPROFILE",
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

struct TokenResult {
  AppCheckToken token;
  int error = -1;
  std::string error_message;
};

// Captures SDK log output so tests can assert the design-mandated diagnostic
// contracts (source attribution, missing app ID, parse errors, 403 hint).
std::string* g_captured_logs = nullptr;
void CaptureLog(LogLevel, const char* message, void*) {
  if (g_captured_logs && message) g_captured_logs->append(message).append("\n");
}

class DebugProviderDesktopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (const char* var : kTrackedEnvVars) {
      const char* val = std::getenv(var);
      saved_env_[var] = {val != nullptr, val ? val : ""};
      SetEnvVar(var, nullptr);
    }

    temp_dir_ = fs::temp_directory_path() / "firebase_appcheck_test";
    fs::remove_all(temp_dir_);
    fs::create_directories(temp_dir_);
    std::string temp_str = temp_dir_.string();
    SetEnvVar("HOME", temp_str.c_str());
    SetEnvVar("USERPROFILE", temp_str.c_str());

    g_captured_logs = &logs_;
    orig_log_level_ = GetLogLevel();
    SetLogLevel(kLogLevelVerbose);
    LogSetCallback(CaptureLog, nullptr);

    DebugAppCheckProviderFactory::GetInstance()->SetDebugToken("");
    rest::SetTransportBuilder([]() -> flatbuffers::unique_ptr<rest::Transport> {
      return flatbuffers::unique_ptr<rest::Transport>(
          new rest::TransportMock());
    });

    AppOptions options = testing::MockAppOptions();
    options.set_project_id(kTestProjectId);
    options.set_app_id(kTestAppId);
    options.set_api_key(kTestApiKey);
    app_ = testing::CreateApp(options);
  }

  void TearDown() override {
    delete app_;
    DebugAppCheckProviderFactory::GetInstance()->SetDebugToken("");
    rest::SetTransportBuilder(nullptr);
    LogSetCallback(nullptr, nullptr);
    SetLogLevel(orig_log_level_);
    g_captured_logs = nullptr;

    std::error_code ec;
    fs::remove_all(temp_dir_, ec);
    for (const auto& entry : saved_env_) {
      SetEnvVar(entry.first.c_str(),
                entry.second.first ? entry.second.second.c_str() : nullptr);
    }
  }

  bool LogContains(const std::string& needle) const {
    return logs_.find(needle) != std::string::npos;
  }
  void ClearLogs() { logs_.clear(); }

  std::string WriteConfigstore(const fs::path& base_dir,
                               const std::string& content) {
    fs::path file_path = base_dir / "configstore" / "firebase-appcheck.json";
    fs::create_directories(file_path.parent_path());
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    out << content;
    return file_path.string();
  }

  std::string WriteHomeConfigstore(const std::string& content) {
    return WriteConfigstore(temp_dir_ / ".config", content);
  }

  TokenResult FetchToken(AppCheckProvider* provider, bool limited_use = false) {
    auto promise = std::make_shared<std::promise<TokenResult>>();
    auto callback = [promise](AppCheckToken tok, int err,
                              const std::string& msg) {
      promise->set_value({tok, err, msg});
    };
    if (limited_use) {
      provider->GetLimitedUseToken(callback);
    } else {
      provider->GetToken(callback);
    }
    auto future = promise->get_future();
    if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
      ADD_FAILURE() << "GetToken callback not invoked within 5s";
      return TokenResult();
    }
    return future.get();
  }

  App* app_ = nullptr;
  fs::path temp_dir_;
  LogLevel orig_log_level_;
  std::string logs_;
  std::map<std::string, std::pair<bool, std::string>> saved_env_;
};

TEST_F(DebugProviderDesktopTest, ResolutionPriorityOrder) {
  // 4. Default fallback: auto-generated 36-char UUID, cached on the provider.
  DebugAppCheckProvider uuid_provider(app_, "");
  std::string uuid = uuid_provider.DetermineDebugToken();
  EXPECT_EQ(uuid.size(), 36u);
  EXPECT_EQ(uuid_provider.DetermineDebugToken(), uuid);

  // 3. Configstore in HOME (~/.config/configstore/firebase-appcheck.json).
  WriteHomeConfigstore(std::string("{\"tokens\":{\"") + kTestAppId +
                       "\":\"home-token\"}}");
  EXPECT_EQ(DebugAppCheckProvider(app_, "").DetermineDebugToken(),
            "home-token");
  // The provider joins with '/' regardless of platform; do not compare
  // against fs::path (which uses '\\' on Windows).
  EXPECT_TRUE(LogContains("Debug token loaded from " + temp_dir_.string() +
                          "/.config/configstore/firebase-appcheck.json"));

  // 3b. XDG_CONFIG_HOME overrides HOME when valid, and falls back to HOME when
  // relative or missing the target app_id.
  fs::path xdg_dir = temp_dir_ / "xdg";
  WriteConfigstore(xdg_dir,
                   "{\"tokens\":{\"1:000:ios:other\":\"other-xdg-token\"}}");
  SetEnvVar("XDG_CONFIG_HOME", xdg_dir.string().c_str());
  EXPECT_EQ(DebugAppCheckProvider(app_, "").DetermineDebugToken(),
            "home-token");

  WriteConfigstore(xdg_dir, std::string("{\"tokens\":{\"") + kTestAppId +
                                "\":\"xdg-token\"}}");
  EXPECT_EQ(DebugAppCheckProvider(app_, "").DetermineDebugToken(), "xdg-token");

  SetEnvVar("XDG_CONFIG_HOME", "relative/xdg_path");
  EXPECT_EQ(DebugAppCheckProvider(app_, "").DetermineDebugToken(),
            "home-token");

  // 2. Environment variables: FIREBASE_APPCHECK_DEBUG_TOKEN >
  // AppCheckDebugToken > APP_CHECK_DEBUG_TOKEN > configstore.
  SetEnvVar("APP_CHECK_DEBUG_TOKEN", "old-cpp-env-token");
  EXPECT_EQ(DebugAppCheckProvider(app_, "").DetermineDebugToken(),
            "old-cpp-env-token");

  SetEnvVar("AppCheckDebugToken", "legacy-env-token");
  EXPECT_EQ(DebugAppCheckProvider(app_, "").DetermineDebugToken(),
            "legacy-env-token");

  SetEnvVar("FIREBASE_APPCHECK_DEBUG_TOKEN", "primary-env-token");
  EXPECT_EQ(DebugAppCheckProvider(app_, "").DetermineDebugToken(),
            "primary-env-token");

  // 1. SetDebugToken takes highest precedence; it is snapshotted when the
  // factory creates the provider.
  auto* factory = DebugAppCheckProviderFactory::GetInstance();
  factory->SetDebugToken("explicit-code-token");
  auto* provider =
      static_cast<DebugAppCheckProvider*>(factory->CreateProvider(app_));
  EXPECT_EQ(provider->DetermineDebugToken(), "explicit-code-token");
}

TEST_F(DebugProviderDesktopTest, ResolveDebugTokenFromConfigEdgeCases) {
  // Missing file (ENOENT) returns empty string and stays silent.
  ClearLogs();
  EXPECT_EQ(ResolveDebugTokenFromConfig(
                (temp_dir_ / "nonexistent.json").string(), kTestAppId),
            "");
  EXPECT_FALSE(LogContains("[FirebaseAppCheck]")) << logs_;

  // UTF-8 BOM prefix is stripped before JSON parsing.
  std::string path =
      WriteHomeConfigstore(std::string("\xEF\xBB\xBF{\"tokens\":{\"") +
                           kTestAppId + "\":\"bom-token\"}}");
  EXPECT_EQ(ResolveDebugTokenFromConfig(path, kTestAppId), "bom-token");

  // File exists but lacks the target app ID -> diagnostic info log.
  path = WriteHomeConfigstore("{\"tokens\":{\"1:000:web:other\":\"tok\"}}");
  EXPECT_EQ(ResolveDebugTokenFromConfig(path, kTestAppId), "");
  EXPECT_TRUE(LogContains(std::string("contains no debug token for app ID '") +
                          kTestAppId + "'"));

  // Malformed JSON and invalid schema variations return "" with a warning.
  const std::string invalid_payloads[] = {
      "{bad",
      "[\"not-an-object\"]",
      "{\"tokens\":\"not-a-map\"}",
      std::string("{\"tokens\":{\"") + kTestAppId + "\":123}}",
      std::string("{\"tokens\":{\"") + kTestAppId + "\":\"\"}}",
  };
  for (const std::string& payload : invalid_payloads) {
    ClearLogs();
    EXPECT_EQ(
        ResolveDebugTokenFromConfig(WriteHomeConfigstore(payload), kTestAppId),
        "")
        << "Payload: " << payload;
    EXPECT_TRUE(LogContains("[FirebaseAppCheck]"))
        << "No diagnostic for payload: " << payload;
  }
}

TEST_F(DebugProviderDesktopTest, GetTokenExchangesTokenAndPropagatesErrors) {
  SetEnvVar("FIREBASE_APPCHECK_DEBUG_TOKEN", "exchange-debug-token");
  DebugAppCheckProvider provider_obj(app_, "");
  AppCheckProvider* provider = &provider_obj;

  std::string ok_config =
      std::string("{config:[{fake:'") + kExpectedExchangeUrl +
      "',httpresponse:{header:['HTTP/1.1 200 OK'],"
      "body:['{\"token\":\"minted-jwt\",\"ttl\":\"3600s\"}']}}]}";
  firebase::testing::cppsdk::ConfigSet(ok_config.c_str());

  TokenResult ok_res = FetchToken(provider, /*limited_use=*/true);
  EXPECT_EQ(ok_res.error, kAppCheckErrorNone);
  EXPECT_EQ(ok_res.token.token, "minted-jwt");

  std::string err_config = std::string("{config:[{fake:'") +
                           kExpectedExchangeUrl +
                           "',httpresponse:{header:['HTTP/1.1 403 Forbidden'],"
                           "body:['{\"error\":{\"code\":403}}']}}]}";
  firebase::testing::cppsdk::ConfigSet(err_config.c_str());

  TokenResult err_res = FetchToken(provider);
  EXPECT_EQ(err_res.error, kAppCheckErrorUnknown);
  EXPECT_TRUE(LogContains(
      "[FirebaseAppCheck] Debug token was rejected by server. Run 'firebase "
      "appcheck:debugtokens:create --force' to provision a new token."));
}

}  // namespace
}  // namespace internal
}  // namespace app_check
}  // namespace firebase
