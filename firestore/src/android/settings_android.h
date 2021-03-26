#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SETTINGS_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SETTINGS_ANDROID_H_

#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {

class SettingsInternal : public jni::Object {
 public:
  using jni::Object::Object;

  static void Initialize(jni::Loader& loader);

  static jni::Local<SettingsInternal> Create(jni::Env& env,
                                             const Settings& settings);

  Settings ToPublic(jni::Env& env) const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SETTINGS_ANDROID_H_
