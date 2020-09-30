#include "firestore/src/android/settings_android.h"

#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;

using SettingsTest = FirestoreIntegrationTest;

TEST_F(SettingsTest, ConverterBoolsAllTrue) {
  Env env;

  Settings settings;
  settings.set_host("foo");
  settings.set_ssl_enabled(true);
  settings.set_persistence_enabled(true);

  Settings result = SettingsInternal::Create(env, settings).ToPublic(env);

  EXPECT_EQ("foo", result.host());
  EXPECT_TRUE(result.is_ssl_enabled());
  EXPECT_TRUE(result.is_persistence_enabled());
}

TEST_F(SettingsTest, ConverterBoolsAllFalse) {
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
