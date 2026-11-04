// Copyright 2020 Google LLC

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

namespace firebase::firestore {
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
    : Wrapper(other) {}

DocumentSnapshotInternal::DocumentSnapshotInternal(
    DocumentSnapshotInternal&& other) noexcept
    : Wrapper(std::move(other)), cached_id_(std::move(other.cached_id_)) {}

void DocumentSnapshotInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kGetId, kGetReference, kGetMetadata, kExists,
                   kGetData, kContains, kGet, kHashCode);
}

Firestore* DocumentSnapshotInternal::firestore() const {
  return firestore_ ? firestore_->firestore_public() : nullptr;
}

const std::string& DocumentSnapshotInternal::id() const {
  std::call_once(id_once_flag_, [this]() {
    if (cached_id_.empty()) {
      Env env = GetEnv();
      cached_id_ = env.Call(obj_, kGetId).ToString(env);
    }
  });
  return cached_id_;
}

DocumentReference DocumentSnapshotInternal::reference() const {
  if (!firestore_) return {};
  Env env = GetEnv();
  Local<Object> ref = env.Call(obj_, kGetReference);
  return (env.ok() && ref) ? firestore_->NewDocumentReference(env, ref)
                           : DocumentReference{};
}

SnapshotMetadata DocumentSnapshotInternal::metadata() const {
  Env env = GetEnv();
  auto java_metadata = env.Call(obj_, kGetMetadata);
  return (env.ok() && java_metadata) ? java_metadata.ToPublic(env)
                                     : SnapshotMetadata{false, false};
}

bool DocumentSnapshotInternal::exists() const {
  Env env = GetEnv();
  return env.Call(obj_, kExists);
}

MapFieldValue DocumentSnapshotInternal::GetData(
    ServerTimestampBehavior stb) const {
  Env env = GetEnv();
  Local<Object> java_data =
      env.Call(obj_, kGetData, ServerTimestampBehaviorInternal::Create(env, stb));
  return (env.ok() && java_data) ? FieldValueInternal(java_data).map_value()
                                 : MapFieldValue();
}

FieldValue DocumentSnapshotInternal::Get(const FieldPath& field,
                                         ServerTimestampBehavior stb) const {
  Env env = GetEnv();
  Local<Object> java_field = FieldPathConverter::Create(env, field);
  Local<Object> field_value = env.Call(
      obj_, kGet, java_field, ServerTimestampBehaviorInternal::Create(env, stb));
  if (!env.ok()) return {};
  if (!field_value) {
    bool contains = env.Call(obj_, kContains, java_field);
    return (env.ok() && contains) ? FieldValue::Null() : FieldValue();
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

}  // namespace firebase::firestore
