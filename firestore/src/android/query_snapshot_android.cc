#include "firestore/src/android/query_snapshot_android.h"

#include "app/src/assert.h"
#include "firestore/src/android/document_change_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/query_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/list.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::List;
using jni::Local;
using jni::Method;
using jni::Object;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/QuerySnapshot";
Method<Object> kGetQuery("getQuery", "()Lcom/google/firebase/firestore/Query;");
Method<SnapshotMetadataInternal> kGetMetadata(
    "getMetadata", "()Lcom/google/firebase/firestore/SnapshotMetadata;");
Method<List> kGetDocumentChanges(
    "getDocumentChanges",
    "(Lcom/google/firebase/firestore/MetadataChanges;)Ljava/util/List;");
Method<List> kGetDocuments("getDocuments", "()Ljava/util/List;");
Method<size_t> kSize("size", "()I");

}  // namespace

void QuerySnapshotInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kGetQuery, kGetMetadata, kGetDocumentChanges,
                   kGetDocuments, kSize);
}

Query QuerySnapshotInternal::query() const {
  Env env = GetEnv();
  Local<Object> query = env.Call(obj_, kGetQuery);
  return firestore_->NewQuery(env, query);
}

SnapshotMetadata QuerySnapshotInternal::metadata() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetMetadata).ToPublic(env);
}

std::vector<DocumentChange> QuerySnapshotInternal::DocumentChanges(
    MetadataChanges metadata_changes) const {
  Env env = GetEnv();
  auto java_metadata = MetadataChangesInternal::Create(env, metadata_changes);

  Local<List> change_list = env.Call(obj_, kGetDocumentChanges, java_metadata);
  return MakePublicVector<DocumentChange>(env, firestore_, change_list);
}

std::vector<DocumentSnapshot> QuerySnapshotInternal::documents() const {
  Env env = GetEnv();
  Local<List> document_list = env.Call(obj_, kGetDocuments);
  return MakePublicVector<DocumentSnapshot>(env, firestore_, document_list);
}

std::size_t QuerySnapshotInternal::size() const {
  Env env = GetEnv();
  return env.Call(obj_, kSize);
}

}  // namespace firestore
}  // namespace firebase
