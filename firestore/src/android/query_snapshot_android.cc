#include "firestore/src/android/query_snapshot_android.h"

#include <jni.h>

#include "app/src/assert.h"
#include "app/src/util_android.h"
#include "firestore/src/android/document_change_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/query_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define QUERY_SNAPSHOT_METHODS(X)                                          \
  X(Query, "getQuery", "()Lcom/google/firebase/firestore/Query;"),         \
  X(Metadata, "getMetadata",                                               \
    "()Lcom/google/firebase/firestore/SnapshotMetadata;"),                 \
  X(DocumentChanges, "getDocumentChanges",                                 \
    "(Lcom/google/firebase/firestore/MetadataChanges;)Ljava/util/List;"),  \
  X(Documents, "getDocuments", "()Ljava/util/List;"),                      \
  X(Size, "size", "()I")
// clang-format on

METHOD_LOOKUP_DECLARATION(query_snapshot, QUERY_SNAPSHOT_METHODS)
METHOD_LOOKUP_DEFINITION(query_snapshot,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/QuerySnapshot",
                         QUERY_SNAPSHOT_METHODS)

Query QuerySnapshotInternal::query() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject query = env->CallObjectMethod(
      obj_, query_snapshot::GetMethodId(query_snapshot::kQuery));
  QueryInternal* internal = new QueryInternal{firestore_, query};
  env->DeleteLocalRef(query);

  CheckAndClearJniExceptions(env);
  return Query{internal};
}

SnapshotMetadata QuerySnapshotInternal::metadata() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject metadata = env->CallObjectMethod(
      obj_, query_snapshot::GetMethodId(query_snapshot::kMetadata));
  SnapshotMetadata result =
      SnapshotMetadataInternal::JavaSnapshotMetadataToSnapshotMetadata(
          env, metadata);

  CheckAndClearJniExceptions(env);
  return result;
}

std::vector<DocumentChange> QuerySnapshotInternal::DocumentChanges(
    MetadataChanges metadata_changes) const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject j_metadata_changes =
      MetadataChangesInternal::ToJavaObject(env, metadata_changes);
  jobject change_list = env->CallObjectMethod(
      obj_, query_snapshot::GetMethodId(query_snapshot::kDocumentChanges),
      j_metadata_changes);
  CheckAndClearJniExceptions(env);

  std::vector<DocumentChange> result;
  JavaListToStdVector<DocumentChange, DocumentChangeInternal>(
      firestore_, change_list, &result);
  return result;
}

std::vector<DocumentSnapshot> QuerySnapshotInternal::documents() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject document_list = env->CallObjectMethod(
      obj_, query_snapshot::GetMethodId(query_snapshot::kDocuments));
  CheckAndClearJniExceptions(env);

  std::vector<DocumentSnapshot> result;
  JavaListToStdVector<DocumentSnapshot, DocumentSnapshotInternal>(
      firestore_, document_list, &result);
  env->DeleteLocalRef(document_list);
  return result;
}

std::size_t QuerySnapshotInternal::size() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jint result = env->CallIntMethod(
      obj_, query_snapshot::GetMethodId(query_snapshot::kSize));

  CheckAndClearJniExceptions(env);
  return static_cast<std::size_t>(result);
}

/* static */
bool QuerySnapshotInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = query_snapshot::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void QuerySnapshotInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  query_snapshot::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
