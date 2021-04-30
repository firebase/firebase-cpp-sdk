#include "firestore/src/jni/integer.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClassName[] = "java/lang/Integer";
Constructor<Integer> kConstructor("(I)V");
Method<int32_t> kIntValue("intValue", "()I");
jclass g_clazz = nullptr;

}  // namespace

void Integer::Initialize(Loader& loader) {
  g_clazz = util::integer_class::GetClass();
  loader.LoadFromExistingClass(kClassName, g_clazz, kConstructor, kIntValue);
}

Class Integer::GetClass() { return Class(g_clazz); }

Local<Integer> Integer::Create(Env& env, int32_t value) {
  return env.New(kConstructor, value);
}

int32_t Integer::IntValue(Env& env) const { return env.Call(*this, kIntValue); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
