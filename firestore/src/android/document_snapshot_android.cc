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

#include "firestore/src/android/document_snapshot_android.h"
#include <cstdint>

#include <utility>

#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/server_timestamp_behavior_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/compare.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::String;

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/DocumentSnapshot";

Method<String> kGetId("getId", "()Ljava/lang/String;");
Method<Object> kGetReference(
    "getReference", "()Lcom/google/firebase/firestore/DocumentReference;");
Method<SnapshotMetadataInternal> kGetMetadata(
    "getMetadata", "()Lcom/google/firebase/firestore/SnapshotMetadata;");
Method<bool> kExists("exists", "()Z");
Method<Object> kGetData("getData",
                        "(Lcom/google/firebase/firestore/DocumentSnapshot$"
                        "ServerTimestampBehavior;)Ljava/util/Map;");
Method<bool> kContains("contains",
                       "(Lcom/google/firebase/firestore/FieldPath;)Z");
Method<Object> kGet("get",
                    "(Lcom/google/firebase/firestore/FieldPath;"
                    "Lcom/google/firebase/firestore/DocumentSnapshot$"
                    "ServerTimestampBehavior;)Ljava/lang/Object;");
Method<int32_t> kHashCode("hashCode", "()I");

}  // namespace

DocumentSnapshotInternal::DocumentSnapshotInternal(
    const DocumentSnapshotInternal& other)
    : Wrapper(other.firestore_, other.obj_) {
  cached_id_ = other.cached_id_;
}

DocumentSnapshotInternal::DocumentSnapshotInternal(
    DocumentSnapshotInternal&& other) noexcept
    : Wrapper(other.firestore_, other.obj_) {
  cached_id_ = std::move(other.cached_id_);
}

void DocumentSnapshotInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kGetId, kGetReference, kGetMetadata, kExists,
                   kGetData, kContains, kGet, kHashCode);
}

Firestore* DocumentSnapshotInternal::firestore() const {
  if (firestore_ == nullptr) {
    return nullptr;
  }
  return firestore_->firestore_public();
}

const std::string& DocumentSnapshotInternal::id() const {
  if (cached_id_.empty()) {
    std::call_once(id_once_flag_, [this]() {
      Env env = GetEnv();
      cached_id_ = env.Call(obj_, kGetId).ToString(env);
    });
  }
  return cached_id_;
}

DocumentReference DocumentSnapshotInternal::reference() const {
  Env env = GetEnv();
  Local<Object> reference = env.Call(obj_, kGetReference);
  return firestore_->NewDocumentReference(env, reference);
}

SnapshotMetadata DocumentSnapshotInternal::metadata() const {
  Env env = GetEnv();
  auto java_metadata = env.Call(obj_, kGetMetadata);
  if (!env.ok() || !java_metadata) {
    return SnapshotMetadata{false, false};
  }
  return java_metadata.ToPublic(env);
}

bool DocumentSnapshotInternal::exists() const {
  Env env = GetEnv();
  return env.Call(obj_, kExists);
}

MapFieldValue DocumentSnapshotInternal::GetData(
    ServerTimestampBehavior stb) const {
  Env env = GetEnv();
  Local<Object> java_stb = ServerTimestampBehaviorInternal::Create(env, stb);
  Local<Object> java_data = env.Call(obj_, kGetData, java_stb);

  if (!env.ok() || !java_data) {
    // If the document doesn't exist, Android's getData() returns a null Map.
    // In C++, the map is returned by value, so translate this case to an empty
    // MapFieldValue.
    return MapFieldValue();
  }

  return FieldValueInternal(java_data).map_value();
}

FieldValue DocumentSnapshotInternal::Get(const FieldPath& field,
                                         ServerTimestampBehavior stb) const {
  Env env = GetEnv();
  Local<Object> java_field = FieldPathConverter::Create(env, field);
  Local<Object> java_stb = ServerTimestampBehaviorInternal::Create(env, stb);
  Local<Object> field_value = env.Call(obj_, kGet, java_field, java_stb);
  if (!env.ok()) {
    return FieldValue();
  }

  if (!field_value) {
    // Android returns null for both null fields and nonexistent fields, so use
    // contains() to check if the field actually exists on the document.
    bool contains_field = env.Call(obj_, kContains, java_field);
    if (!env.ok() || !contains_field) {
      return FieldValue();
    }
    return FieldValue::Null();
  }

  return FieldValueInternal::Create(env, field_value);
}

std::size_t DocumentSnapshotInternal::Hash() const {
  Env env = GetEnv();
  return static_cast<uint32_t>(env.Call(obj_, kHashCode));
}

bool operator==(const DocumentSnapshotInternal& lhs,
                const DocumentSnapshotInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firestore
}  // namespace firebase
