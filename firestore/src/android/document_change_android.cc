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

#include "firestore/src/android/document_change_android.h"

#include "firestore/src/android/document_change_type_android.h"
#include "firestore/src/android/document_snapshot_android.h"
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

using Type = DocumentChange::Type;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/DocumentChange";
Method<DocumentChangeTypeInternal> kType(
    "getType", "()Lcom/google/firebase/firestore/DocumentChange$Type;");
Method<Object> kDocument(
    "getDocument", "()Lcom/google/firebase/firestore/QueryDocumentSnapshot;");
Method<size_t> kOldIndex("getOldIndex", "()I");
Method<size_t> kNewIndex("getNewIndex", "()I");
Method<int32_t> kHashCode("hashCode", "()I");

}  // namespace

void DocumentChangeInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kType, kDocument, kOldIndex, kNewIndex, kHashCode);
}

Type DocumentChangeInternal::type() const {
  Env env = GetEnv();
  Local<DocumentChangeTypeInternal> type = env.Call(obj_, kType);
  return type.GetType(env);
}

DocumentSnapshot DocumentChangeInternal::document() const {
  Env env = GetEnv();
  Local<Object> snapshot = env.Call(obj_, kDocument);
  return firestore_->NewDocumentSnapshot(env, snapshot);
}

std::size_t DocumentChangeInternal::old_index() const {
  Env env = GetEnv();
  return env.Call(obj_, kOldIndex);
}

std::size_t DocumentChangeInternal::new_index() const {
  Env env = GetEnv();
  return env.Call(obj_, kNewIndex);
}

std::size_t DocumentChangeInternal::Hash() const {
  Env env = GetEnv();
  return env.Call(obj_, kHashCode);
}

bool operator==(const DocumentChangeInternal& lhs,
                const DocumentChangeInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firestore
}  // namespace firebase
