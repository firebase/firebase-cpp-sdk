#include "firestore/src/android/document_snapshot_android.h"

#include <utility>

#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/server_timestamp_behavior_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Map;
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

}  // namespace

void DocumentSnapshotInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kGetId, kGetReference, kGetMetadata, kExists,
                   kGetData, kContains, kGet);
}

Firestore* DocumentSnapshotInternal::firestore() const {
  FIREBASE_ASSERT(firestore_->firestore_public() != nullptr);
  return firestore_->firestore_public();
}

const std::string& DocumentSnapshotInternal::id() const {
  if (cached_id_.empty()) {
    Env env = GetEnv();
    cached_id_ = env.Call(obj_, kGetId).ToString(env);
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

  if (!java_data) {
    // If the document doesn't exist, Android returns a null Map. In C++, the
    // map is returned by value, so translate this case to an empty map.
    return MapFieldValue();
  }

  return FieldValueInternal(java_data).map_value();
}

FieldValue DocumentSnapshotInternal::Get(const FieldPath& field,
                                         ServerTimestampBehavior stb) const {
  Env env = GetEnv();
  Local<Object> java_field = FieldPathConverter::Create(env, field);

  // Android returns null for both null fields and nonexistent fields, so first
  // use contains() to check if the field exists.
  bool contains_field = env.Call(obj_, kContains, java_field);
  if (!contains_field) {
    return FieldValue();
  }

  Local<Object> java_stb = ServerTimestampBehaviorInternal::Create(env, stb);
  Local<Object> field_value = env.Call(obj_, kGet, java_field, java_stb);
  return FieldValueInternal::Create(env, field_value);
}

}  // namespace firestore
}  // namespace firebase
