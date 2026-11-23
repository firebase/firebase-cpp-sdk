// Copyright 2026 Google LLC
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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "firebase/app_check.h"
#include "gtest/gtest.h"

namespace firebase {
namespace app_check {
namespace {

// Hands out a token that names the App the provider was created for.
class NamedTokenProvider : public AppCheckProvider {
 public:
  explicit NamedTokenProvider(App* app)
      : token_(std::string("token-for-") + app->name()) {}

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override {
    AppCheckToken token;
    token.token = token_;
    token.expire_time_millis = 4102444800000LL;  // 2100-01-01.
    completion_callback(token, kAppCheckErrorNone, "");
  }

 private:
  std::string token_;
};

class NamedTokenProviderFactory : public AppCheckProviderFactory {
 public:
  AppCheckProvider* CreateProvider(App* app) override {
    providers_.push_back(std::make_unique<NamedTokenProvider>(app));
    return providers_.back().get();
  }

 private:
  std::vector<std::unique_ptr<NamedTokenProvider>> providers_;
};

class AppCheckDesktopTest : public ::testing::Test {
 protected:
  void SetUp() override { AppCheck::SetAppCheckProviderFactory(&factory_); }
  void TearDown() override { AppCheck::SetAppCheckProviderFactory(nullptr); }

  NamedTokenProviderFactory factory_;
};

// Regression tests for
// https://github.com/firebase/firebase-cpp-sdk/issues/1970. Other products look
// up App Check through the function registry of their own App, so every App
// with an AppCheck instance needs the functions registered.
TEST_F(AppCheckDesktopTest, RegistersRegistryFunctionsOnEveryApp) {
  std::unique_ptr<App> default_app(testing::CreateApp());
  std::unique_ptr<App> second_app(
      testing::CreateApp(testing::MockAppOptions(), "second"));
  std::unique_ptr<AppCheck> default_app_check(
      AppCheck::GetInstance(default_app.get()));
  std::unique_ptr<AppCheck> second_app_check(
      AppCheck::GetInstance(second_app.get()));

  for (::firebase::internal::FunctionId id :
       {::firebase::internal::FnAppCheckGetTokenAsync,
        ::firebase::internal::FnAppCheckGetLimitedUseTokenAsync,
        ::firebase::internal::FnAppCheckAddListener,
        ::firebase::internal::FnAppCheckRemoveListener}) {
    EXPECT_TRUE(default_app->function_registry()->FunctionExists(id)) << id;
    EXPECT_TRUE(second_app->function_registry()->FunctionExists(id)) << id;
  }
}

TEST_F(AppCheckDesktopTest, EachAppGetsItsOwnTokenThroughItsRegistry) {
  std::unique_ptr<App> default_app(testing::CreateApp());
  std::unique_ptr<App> second_app(
      testing::CreateApp(testing::MockAppOptions(), "second"));
  std::unique_ptr<AppCheck> default_app_check(
      AppCheck::GetInstance(default_app.get()));
  std::unique_ptr<AppCheck> second_app_check(
      AppCheck::GetInstance(second_app.get()));

  for (App* app : {default_app.get(), second_app.get()}) {
    Future<std::string> token;
    ASSERT_TRUE(app->function_registry()->CallFunction(
        ::firebase::internal::FnAppCheckGetTokenAsync, app, nullptr, &token))
        << app->name();
    ASSERT_EQ(token.status(), kFutureStatusComplete) << app->name();
    ASSERT_EQ(token.error(), kAppCheckErrorNone) << app->name();
    EXPECT_EQ(*token.result(), std::string("token-for-") + app->name());
  }
}

}  // namespace
}  // namespace app_check
}  // namespace firebase
