#include "firestore/src/jni/double.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClassName[] = "java/lang/Double";
Constructor<Double> kConstructor("(D)V");
Method<double> kDoubleValue("doubleValue", "()D");
jclass g_clazz = nullptr;

}  // namespace

void Double::Initialize(Loader& loader) {
  g_clazz = util::double_class::GetClass();
  loader.LoadFromExistingClass(kClassName, g_clazz, kConstructor, kDoubleValue);
}

Class Double::GetClass() { return Class(g_clazz); }

Local<Double> Double::Create(Env& env, double value) {
  return env.New(kConstructor, value);
}

double Double::DoubleValue(Env& env) const {
  return env.Call(*this, kDoubleValue);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
