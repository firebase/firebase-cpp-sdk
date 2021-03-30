#include "firestore/src/android/firebase_firestore_settings_android.h"

#include <jni.h>

#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

TEST_F(FirestoreIntegrationTest, ConverterBoolsAllTrue) {
  JNIEnv* env = app()->GetJNIEnv();

  Settings settings;
  settings.set_host("foo");
  settings.set_ssl_enabled(true);
  settings.set_persistence_enabled(true);
  jobject java_settings =
      FirebaseFirestoreSettingsInternal::SettingToJavaSetting(env, settings);
  const Settings result =
      FirebaseFirestoreSettingsInternal::JavaSettingToSetting(env,
                                                              java_settings);
  EXPECT_EQ("foo", result.host());
  EXPECT_TRUE(result.is_ssl_enabled());
  EXPECT_TRUE(result.is_persistence_enabled());

  env->DeleteLocalRef(java_settings);
}

TEST_F(FirestoreIntegrationTest, ConverterBoolsAllFalse) {
  JNIEnv* env = app()->GetJNIEnv();

  Settings settings;
  settings.set_host("bar");
  settings.set_ssl_enabled(false);
  settings.set_persistence_enabled(false);
  jobject java_settings =
      FirebaseFirestoreSettingsInternal::SettingToJavaSetting(env, settings);
  const Settings result =
      FirebaseFirestoreSettingsInternal::JavaSettingToSetting(env,
                                                              java_settings);
  EXPECT_EQ("bar", result.host());
  EXPECT_FALSE(result.is_ssl_enabled());
  EXPECT_FALSE(result.is_persistence_enabled());

  env->DeleteLocalRef(java_settings);
}

}  // namespace firestore
}  // namespace firebase
