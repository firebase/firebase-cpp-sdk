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
Method<String> kToString("toString", "()Ljava/lang/String;");
jclass object_class = nullptr;

}  // namespace

void Object::Initialize(Loader& loader) {
  object_class = util::object::GetClass();
  loader.LoadFromExistingClass("java/lang/Object", object_class, kEquals,
                               kToString);
}

Class Object::GetClass() { return Class(object_class); }

std::string Object::ToString(Env& env) const {
  Local<String> java_string = env.Call(*this, kToString);
  return java_string.ToString(env);
}

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
