// Copyright 2020 Google LLC

#include "firestore/src/android/query_snapshot_android.h"

#include <cstdint>

#include "firestore/src/android/document_change_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/jni/compare.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/list.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore {
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
Method<int32_t> kSize("size", "()I");
Method<int32_t> kHashCode("hashCode", "()I");

}  // namespace

void QuerySnapshotInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kGetQuery, kGetMetadata, kGetDocumentChanges,
                   kGetDocuments, kSize, kHashCode);
}

Query QuerySnapshotInternal::query() const {
  Env env = GetEnv();
  return firestore_->NewQuery(env, env.Call(obj_, kGetQuery));
}

SnapshotMetadata QuerySnapshotInternal::metadata() const {
  Env env = GetEnv();
  Local<SnapshotMetadataInternal> metadata = env.Call(obj_, kGetMetadata);
  return (env.ok() && metadata) ? metadata.ToPublic(env)
                                : SnapshotMetadata(false, false);
}

std::vector<DocumentChange> QuerySnapshotInternal::DocumentChanges(
    MetadataChanges metadata_changes) const {
  Env env = GetEnv();
  Local<List> changes =
      env.Call(obj_, kGetDocumentChanges,
               MetadataChangesInternal::Create(env, metadata_changes));
  return (env.ok() && changes)
             ? MakePublicVector<DocumentChange>(env, firestore_, changes)
             : std::vector<DocumentChange>{};
}

std::vector<DocumentSnapshot> QuerySnapshotInternal::documents() const {
  Env env = GetEnv();
  Local<List> docs = env.Call(obj_, kGetDocuments);
  return (env.ok() && docs)
             ? MakePublicVector<DocumentSnapshot>(env, firestore_, docs)
             : std::vector<DocumentSnapshot>{};
}

std::size_t QuerySnapshotInternal::size() const {
  Env env = GetEnv();
  int32_t result = env.Call(obj_, kSize);
  return (env.ok() && result >= 0) ? static_cast<std::size_t>(result) : 0;
}

std::size_t QuerySnapshotInternal::Hash() const {
  Env env = GetEnv();
  return static_cast<uint32_t>(env.Call(obj_, kHashCode));
}

bool operator==(const QuerySnapshotInternal& lhs,
                const QuerySnapshotInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firebase::firestore
