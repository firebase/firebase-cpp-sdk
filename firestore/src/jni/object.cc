#include "firestore/src/jni/object.h"

#include "app/src/util_android.h"

namespace firebase {
namespace firestore {
namespace jni {

std::string Object::ToString(JNIEnv* env) const {
  return util::JniObjectToString(env, object_);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
