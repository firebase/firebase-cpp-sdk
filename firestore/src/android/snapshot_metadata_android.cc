#include "firestore/src/android/snapshot_metadata_android.h"

#include <stdint.h>

#include "app/src/util_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define SNAPSHOT_METADATA_METHODS(X)                            \
  X(Constructor, "<init>", "(ZZ)V", util::kMethodTypeInstance), \
  X(HasPendingWrites, "hasPendingWrites", "()Z"),               \
  X(IsFromCache, "isFromCache", "()Z")
// clang-format on

METHOD_LOOKUP_DECLARATION(snapshot_metadata, SNAPSHOT_METADATA_METHODS)
METHOD_LOOKUP_DEFINITION(snapshot_metadata,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/SnapshotMetadata",
                         SNAPSHOT_METADATA_METHODS)

/* static */
jobject SnapshotMetadataInternal::SnapshotMetadataToJavaSnapshotMetadata(
    JNIEnv* env, const SnapshotMetadata& metadata) {
  jobject result = env->NewObject(
      snapshot_metadata::GetClass(),
      snapshot_metadata::GetMethodId(snapshot_metadata::kConstructor),
      static_cast<jboolean>(metadata.has_pending_writes()),
      static_cast<jboolean>(metadata.is_from_cache()));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
SnapshotMetadata
SnapshotMetadataInternal::JavaSnapshotMetadataToSnapshotMetadata(JNIEnv* env,
                                                                 jobject obj) {
  jboolean has_pending_writes = env->CallBooleanMethod(
      obj,
      snapshot_metadata::GetMethodId(snapshot_metadata::kHasPendingWrites));
  jboolean is_from_cache = env->CallBooleanMethod(
      obj, snapshot_metadata::GetMethodId(snapshot_metadata::kIsFromCache));
  env->DeleteLocalRef(obj);
  CheckAndClearJniExceptions(env);
  return SnapshotMetadata{static_cast<bool>(has_pending_writes),
                          static_cast<bool>(is_from_cache)};
}

/* static */
bool SnapshotMetadataInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = snapshot_metadata::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void SnapshotMetadataInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  snapshot_metadata::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
