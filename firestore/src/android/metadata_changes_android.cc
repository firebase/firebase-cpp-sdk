#include "firestore/src/android/metadata_changes_android.h"


namespace firebase {
namespace firestore {

// clang-format off
#define METADATA_CHANGES_METHODS(X) X(Name, "name", "()Ljava/lang/String;")
#define METADATA_CHANGES_FIELDS(X)                                          \
  X(Exclude, "EXCLUDE", "Lcom/google/firebase/firestore/MetadataChanges;",  \
    util::kFieldTypeStatic),                                                \
  X(Include, "INCLUDE", "Lcom/google/firebase/firestore/MetadataChanges;",  \
    util::kFieldTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(metadata_changes, METADATA_CHANGES_METHODS,
                          METADATA_CHANGES_FIELDS)
METHOD_LOOKUP_DEFINITION(metadata_changes,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/MetadataChanges",
                         METADATA_CHANGES_METHODS, METADATA_CHANGES_FIELDS)

jobject MetadataChangesInternal::exclude_ = nullptr;
jobject MetadataChangesInternal::include_ = nullptr;

/* static */
jobject MetadataChangesInternal::ToJavaObject(
    JNIEnv* env, MetadataChanges metadata_changes) {
  if (metadata_changes == MetadataChanges::kExclude) {
    return exclude_;
  } else {
    return include_;
  }
}

/* static */
bool MetadataChangesInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = metadata_changes::CacheMethodIds(env, activity) &&
                metadata_changes::CacheFieldIds(env, activity);
  util::CheckAndClearJniExceptions(env);

  // Cache Java enum values.
  jobject value = env->GetStaticObjectField(
      metadata_changes::GetClass(),
      metadata_changes::GetFieldId(metadata_changes::kExclude));
  exclude_ = env->NewGlobalRef(value);
  env->DeleteLocalRef(value);

  value = env->GetStaticObjectField(
      metadata_changes::GetClass(),
      metadata_changes::GetFieldId(metadata_changes::kInclude));
  include_ = env->NewGlobalRef(value);
  env->DeleteLocalRef(value);

  return result;
}

/* static */
void MetadataChangesInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  metadata_changes::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);

  // Uncache Java enum values.
  env->DeleteGlobalRef(exclude_);
  exclude_ = nullptr;
  env->DeleteGlobalRef(include_);
  include_ = nullptr;
}

}  // namespace firestore
}  // namespace firebase
