// Copyright 2020 Google LLC

#include "firestore/src/android/document_change_android.h"

#include <cstdint>

#include "firestore/src/android/document_change_type_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/jni/compare.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using Type = DocumentChange::Type;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/DocumentChange";
Method<DocumentChangeTypeInternal> kType(
    "getType", "()Lcom/google/firebase/firestore/DocumentChange$Type;");
Method<Object> kDocument(
    "getDocument", "()Lcom/google/firebase/firestore/QueryDocumentSnapshot;");
Method<int32_t> kOldIndex("getOldIndex", "()I");
Method<int32_t> kNewIndex("getNewIndex", "()I");
Method<int32_t> kHashCode("hashCode", "()I");

}  // namespace

void DocumentChangeInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kType, kDocument, kOldIndex, kNewIndex, kHashCode);
}

Type DocumentChangeInternal::type() const {
  Env env = GetEnv();
  Local<DocumentChangeTypeInternal> type = env.Call(obj_, kType);
  return (env.ok() && type) ? type.GetType(env) : Type::kAdded;
}

DocumentSnapshot DocumentChangeInternal::document() const {
  Env env = GetEnv();
  Local<Object> snapshot = env.Call(obj_, kDocument);
  return (env.ok() && snapshot)
             ? firestore_->NewDocumentSnapshot(env, snapshot)
             : DocumentSnapshot();
}

std::size_t DocumentChangeInternal::old_index() const {
  Env env = GetEnv();
  int32_t index = env.Call(obj_, kOldIndex);
  return (env.ok() && index >= 0) ? static_cast<std::size_t>(index)
                                  : DocumentChange::npos;
}

std::size_t DocumentChangeInternal::new_index() const {
  Env env = GetEnv();
  int32_t index = env.Call(obj_, kNewIndex);
  return (env.ok() && index >= 0) ? static_cast<std::size_t>(index)
                                  : DocumentChange::npos;
}

std::size_t DocumentChangeInternal::Hash() const {
  Env env = GetEnv();
  return static_cast<uint32_t>(env.Call(obj_, kHashCode));
}

bool operator==(const DocumentChangeInternal& lhs,
                const DocumentChangeInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firebase::firestore
