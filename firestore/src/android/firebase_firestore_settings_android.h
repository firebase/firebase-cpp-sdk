#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIREBASE_FIRESTORE_SETTINGS_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIREBASE_FIRESTORE_SETTINGS_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firestore/src/include/firebase/firestore/settings.h"

namespace firebase {
namespace firestore {

class FirebaseFirestoreSettingsInternal {
 public:
  using ApiType = Settings;

  static jobject SettingToJavaSetting(JNIEnv* env, const Settings& settings);

  static Settings JavaSettingToSetting(JNIEnv* env, jobject obj);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIREBASE_FIRESTORE_SETTINGS_ANDROID_H_
