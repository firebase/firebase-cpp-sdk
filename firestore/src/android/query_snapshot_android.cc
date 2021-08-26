/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/android/query_snapshot_android.h"

#include "app/src/assert.h"
#include "firestore/src/android/document_change_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/query_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/jni/compare.h"
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
Method<int32_t> kHashCode("hashCode", "()I");

}  // namespace

void QuerySnapshotInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kGetQuery, kGetMetadata, kGetDocumentChanges,
                   kGetDocuments, kSize, kHashCode);
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

std::size_t QuerySnapshotInternal::Hash() const {
  Env env = GetEnv();
  return env.Call(obj_, kHashCode);
}

bool operator==(const QuerySnapshotInternal& lhs,
                const QuerySnapshotInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firestore
}  // namespace firebase
