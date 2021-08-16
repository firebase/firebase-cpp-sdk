/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/android/settings_android.h"

#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/jni/env.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;

using SettingsTestAndroid = FirestoreIntegrationTest;

TEST_F(SettingsTestAndroid, ConverterBoolsAllTrue) {
  Env env;

  Settings settings;
  settings.set_host("foo");
  settings.set_ssl_enabled(true);
  settings.set_persistence_enabled(true);
  int64_t five_mb = 5 * 1024 * 1024;
  settings.set_cache_size_bytes(five_mb);

  Settings result = SettingsInternal::Create(env, settings).ToPublic(env);

  EXPECT_EQ(result.host(), "foo");
  EXPECT_TRUE(result.is_ssl_enabled());
  EXPECT_TRUE(result.is_persistence_enabled());
  EXPECT_EQ(result.cache_size_bytes(), five_mb);
}

TEST_F(SettingsTestAndroid, ConverterBoolsAllFalse) {
  Env env;

  Settings settings;
  settings.set_host("bar");
  settings.set_ssl_enabled(false);
  settings.set_persistence_enabled(false);

  Settings result = SettingsInternal::Create(env, settings).ToPublic(env);

  EXPECT_EQ("bar", result.host());
  EXPECT_FALSE(result.is_ssl_enabled());
  EXPECT_FALSE(result.is_persistence_enabled());
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
