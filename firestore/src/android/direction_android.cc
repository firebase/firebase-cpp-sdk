#include "firestore/src/android/direction_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Object;
using jni::StaticField;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/Query$Direction";
StaticField<Object> kAscending(
    "ASCENDING", "Lcom/google/firebase/firestore/Query$Direction;");
StaticField<Object> kDescending(
    "DESCENDING", "Lcom/google/firebase/firestore/Query$Direction;");

}  // namespace

void DirectionInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kAscending, kDescending);
}

Local<Object> DirectionInternal::Create(Env& env, Query::Direction direction) {
  if (direction == Query::Direction::kAscending) {
    return env.Get(kAscending);
  } else {
    return env.Get(kDescending);
  }
}

}  // namespace firestore
}  // namespace firebase
