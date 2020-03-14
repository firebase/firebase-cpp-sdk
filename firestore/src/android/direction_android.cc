#include "firestore/src/android/direction_android.h"


namespace firebase {
namespace firestore {

// clang-format off
#define DIRECTION_METHODS(X)                               \
  X(Name, "name", "()Ljava/lang/String;")
#define DIRECTION_FIELDS(X)                                \
  X(Ascending, "ASCENDING",                                \
    "Lcom/google/firebase/firestore/Query$Direction;",     \
    util::kFieldTypeStatic),                               \
  X(Descending, "DESCENDING",                              \
    "Lcom/google/firebase/firestore/Query$Direction;",     \
    util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(direction, DIRECTION_METHODS, DIRECTION_FIELDS)
METHOD_LOOKUP_DEFINITION(direction,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/Query$Direction",
                         DIRECTION_METHODS, DIRECTION_FIELDS)

jobject DirectionInternal::ascending_ = nullptr;
jobject DirectionInternal::descending_ = nullptr;

/* static */
jobject DirectionInternal::ToJavaObject(JNIEnv* env,
                                        Query::Direction direction) {
  if (direction == Query::Direction::kAscending) {
    return ascending_;
  } else {
    return descending_;
  }
}

/* static */
bool DirectionInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = direction::CacheMethodIds(env, activity) &&
                direction::CacheFieldIds(env, activity);
  util::CheckAndClearJniExceptions(env);

  // Cache Java enum values.
  jobject value = env->GetStaticObjectField(
      direction::GetClass(), direction::GetFieldId(direction::kAscending));
  ascending_ = env->NewGlobalRef(value);
  env->DeleteLocalRef(value);

  value = env->GetStaticObjectField(
      direction::GetClass(), direction::GetFieldId(direction::kDescending));
  descending_ = env->NewGlobalRef(value);
  env->DeleteLocalRef(value);
  util::CheckAndClearJniExceptions(env);

  return result;
}

/* static */
void DirectionInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  direction::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);

  // Uncache Java enum values.
  env->DeleteGlobalRef(ascending_);
  ascending_ = nullptr;
  env->DeleteGlobalRef(descending_);
  descending_ = nullptr;
}

}  // namespace firestore
}  // namespace firebase
