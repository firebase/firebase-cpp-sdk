#include "firestore/src/android/document_snapshot_android.h"

#include <jni.h>

#include <utility>

#include "app/src/util_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/server_timestamp_behavior_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/include/firebase/firestore.h"

namespace firebase {
namespace firestore {

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

// clang-format off
#define DOCUMENT_SNAPSHOT_METHODS(X)                            \
  X(GetId, "getId", "()Ljava/lang/String;"),                    \
  X(GetReference, "getReference",                               \
    "()Lcom/google/firebase/firestore/DocumentReference;"),     \
  X(GetMetadata, "getMetadata",                                 \
    "()Lcom/google/firebase/firestore/SnapshotMetadata;"),      \
  X(Exists, "exists", "()Z"),                                   \
  X(GetData, "getData",                                         \
    "(Lcom/google/firebase/firestore/DocumentSnapshot$"         \
    "ServerTimestampBehavior;)Ljava/util/Map;"),                \
  X(Contains, "contains",                                       \
    "(Lcom/google/firebase/firestore/FieldPath;)Z"),            \
  X(Get, "get",                                                 \
    "(Lcom/google/firebase/firestore/FieldPath;"                \
    "Lcom/google/firebase/firestore/DocumentSnapshot$"          \
    "ServerTimestampBehavior;)Ljava/lang/Object;")
// clang-format on

METHOD_LOOKUP_DECLARATION(document_snapshot, DOCUMENT_SNAPSHOT_METHODS)
METHOD_LOOKUP_DEFINITION(document_snapshot,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/DocumentSnapshot",
                         DOCUMENT_SNAPSHOT_METHODS)

Firestore* DocumentSnapshotInternal::firestore() const {
  FIREBASE_ASSERT(firestore_->firestore_public() != nullptr);
  return firestore_->firestore_public();
}

const std::string& DocumentSnapshotInternal::id() const {
  if (!cached_id_.empty()) {
    return cached_id_;
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jstring id = static_cast<jstring>(env->CallObjectMethod(
      obj_, document_snapshot::GetMethodId(document_snapshot::kGetId)));
  cached_id_ = util::JniStringToString(env, id);

  CheckAndClearJniExceptions(env);
  return cached_id_;
}

DocumentReference DocumentSnapshotInternal::reference() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject reference = env->CallObjectMethod(
      obj_, document_snapshot::GetMethodId(document_snapshot::kGetReference));
  DocumentReferenceInternal* internal =
      new DocumentReferenceInternal{firestore_, reference};
  env->DeleteLocalRef(reference);

  CheckAndClearJniExceptions(env);
  return DocumentReference{internal};
}

SnapshotMetadata DocumentSnapshotInternal::metadata() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject metadata = env->CallObjectMethod(
      obj_, document_snapshot::GetMethodId(document_snapshot::kGetMetadata));
  SnapshotMetadata result =
      SnapshotMetadataInternal::JavaSnapshotMetadataToSnapshotMetadata(
          env, metadata);

  CheckAndClearJniExceptions(env);
  return result;
}

bool DocumentSnapshotInternal::exists() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jboolean exists = env->CallBooleanMethod(
      obj_, document_snapshot::GetMethodId(document_snapshot::kExists));

  CheckAndClearJniExceptions(env);
  return static_cast<bool>(exists);
}

MapFieldValue DocumentSnapshotInternal::GetData(
    ServerTimestampBehavior stb) const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject stb_enum = ServerTimestampBehaviorInternal::ToJavaObject(env, stb);

  jobject map_value = env->CallObjectMethod(
      obj_, document_snapshot::GetMethodId(document_snapshot::kGetData),
      stb_enum);
  CheckAndClearJniExceptions(env);

  FieldValueInternal value(firestore_, map_value);
  env->DeleteLocalRef(map_value);
  return value.map_value();
}

FieldValue DocumentSnapshotInternal::Get(const FieldPath& field,
                                         ServerTimestampBehavior stb) const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject field_path = FieldPathConverter::ToJavaObject(env, field);

  // Android returns null for both null fields and nonexistent fields, so first
  // use contains() to check if the field exists.
  jboolean contains_field = env->CallBooleanMethod(
      obj_, document_snapshot::GetMethodId(document_snapshot::kContains),
      field_path);
  CheckAndClearJniExceptions(env);
  if (!contains_field) {
    env->DeleteLocalRef(field_path);
    return FieldValue();
  } else {
    jobject stb_enum = ServerTimestampBehaviorInternal::ToJavaObject(env, stb);

    jobject field_value = env->CallObjectMethod(
        obj_, document_snapshot::GetMethodId(document_snapshot::kGet),
        field_path, stb_enum);
    CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(field_path);

    FieldValue result{new FieldValueInternal{firestore_, field_value}};
    env->DeleteLocalRef(field_value);
    return result;
  }
}

/* static */
bool DocumentSnapshotInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = document_snapshot::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void DocumentSnapshotInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  document_snapshot::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
