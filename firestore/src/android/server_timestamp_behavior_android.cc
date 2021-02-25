#include "firestore/src/android/server_timestamp_behavior_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Object;
using jni::StaticField;

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

constexpr char kClass[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/DocumentSnapshot$ServerTimestampBehavior";

StaticField<Object> kNone(
    "NONE",
    "Lcom/google/firebase/firestore/DocumentSnapshot$ServerTimestampBehavior;");
StaticField<Object> kEstimate(
    "ESTIMATE",
    "Lcom/google/firebase/firestore/DocumentSnapshot$ServerTimestampBehavior;");
StaticField<Object> kPrevious(
    "PREVIOUS",
    "Lcom/google/firebase/firestore/DocumentSnapshot$ServerTimestampBehavior;");

}  // namespace

Local<Object> ServerTimestampBehaviorInternal::Create(
    Env& env, ServerTimestampBehavior stb) {
  static_assert(
      ServerTimestampBehavior::kDefault == ServerTimestampBehavior::kNone,
      "default should be the same as none");

  switch (stb) {
    case ServerTimestampBehavior::kNone:
      return env.Get(kNone);
    case ServerTimestampBehavior::kEstimate:
      return env.Get(kEstimate);
    case ServerTimestampBehavior::kPrevious:
      return env.Get(kPrevious);
  }
}

void ServerTimestampBehaviorInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kNone, kEstimate, kPrevious);
}

}  // namespace firestore
}  // namespace firebase
