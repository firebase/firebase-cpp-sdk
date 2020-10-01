#include "firestore/src/android/timestamp_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Class;
using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/Timestamp";
Constructor<TimestampInternal> kConstructor("(JI)V");
Method<int64_t> kGetSeconds("getSeconds", "()J");
Method<int32_t> kGetNanoseconds("getNanoseconds", "()I");

jclass g_clazz = nullptr;

}  // namespace

void TimestampInternal::Initialize(jni::Loader& loader) {
  g_clazz =
      loader.LoadClass(kClassName, kConstructor, kGetSeconds, kGetNanoseconds);
}

Class TimestampInternal::GetClass() { return Class(g_clazz); }

Local<TimestampInternal> TimestampInternal::Create(Env& env,
                                                   const Timestamp& timestamp) {
  return env.New(kConstructor, timestamp.seconds(), timestamp.nanoseconds());
}

Timestamp TimestampInternal::ToPublic(Env& env) const {
  int64_t seconds = env.Call(*this, kGetSeconds);
  int32_t nanoseconds = env.Call(*this, kGetNanoseconds);
  return Timestamp(seconds, nanoseconds);
}

}  // namespace firestore
}  // namespace firebase
