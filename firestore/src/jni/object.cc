#include "firestore/src/jni/object.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Method<bool> kEquals("equals", "(Ljava/lang/Object;)Z");
jclass g_clazz = nullptr;

}  // namespace

void Object::Initialize(Loader& loader) {
  g_clazz = util::object::GetClass();
  loader.LoadFromExistingClass("java/lang/Object", g_clazz, kEquals);
}

Class Object::GetClass() { return Class(util::object::GetClass()); }

std::string Object::ToString(JNIEnv* env) const {
  return util::JniObjectToString(env, object_);
}

std::string Object::ToString(Env& env) const { return ToString(env.get()); }

bool Object::Equals(Env& env, const Object& other) const {
  return env.Call(*this, kEquals, other);
}

bool Object::Equals(Env& env, const Object& lhs, const Object& rhs) {
  // Most likely only happens when comparing one with itself or both are null.
  if (lhs.get() == rhs.get()) return true;

  // If only one of them is nullptr, then they cannot equal.
  if (!lhs || !rhs) return false;

  return lhs.Equals(env, rhs);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
