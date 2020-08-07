#include "firestore/src/jni/object.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace jni {

Class Object::GetClass() { return Class(util::object::GetClass()); }

std::string Object::ToString(JNIEnv* env) const {
  return util::JniObjectToString(env, object_);
}

std::string Object::ToString(Env& env) const { return ToString(env.get()); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
