#include "firestore/src/jni/long.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClassName[] = "java/lang/Long";
Constructor<Long> kConstructor("(J)V");
Method<int64_t> kLongValue("longValue", "()J");
jclass g_clazz = nullptr;

}  // namespace

void Long::Initialize(Loader& loader) {
  g_clazz = util::long_class::GetClass();
  loader.LoadFromExistingClass(kClassName, g_clazz, kConstructor, kLongValue);
}

Class Long::GetClass() { return Class(g_clazz); }

Local<Long> Long::Create(Env& env, int64_t value) {
  return env.New(kConstructor, value);
}

int64_t Long::LongValue(Env& env) const { return env.Call(*this, kLongValue); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
