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

#include "firestore/src/android/collection_reference_android.h"

#include <string>
#include <utility>

#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::HashMap;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::String;
using jni::Task;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/CollectionReference";

Method<String> kGetId("getId", "()Ljava/lang/String;");
Method<String> kGetPath("getPath", "()Ljava/lang/String;");
Method<Object> kGetParent(
    "getParent", "()Lcom/google/firebase/firestore/DocumentReference;");
Method<Object> kDocumentAutoId(
    "document", "()Lcom/google/firebase/firestore/DocumentReference;");
Method<Object> kDocument("document",
                         "(Ljava/lang/String;)"
                         "Lcom/google/firebase/firestore/DocumentReference;");
Method<Task> kAdd("add",
                  "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;");

}  // namespace

CollectionReferenceInternal::CollectionReferenceInternal(
    const CollectionReferenceInternal& other) try
    : QueryInternal((other.mutex_.lock(), other)) {
  cached_id_ = other.cached_id_;
  cached_path_ = other.cached_path_;
  other.mutex_.unlock();
} catch (...) {
  other.mutex_.unlock();
  throw;
}

CollectionReferenceInternal::CollectionReferenceInternal(
    CollectionReferenceInternal&& other) noexcept
    : QueryInternal((other.mutex_.lock(), std::move(other))) {
  cached_id_ = other.cached_id_;
  cached_path_ = other.cached_path_;
  other.mutex_.unlock();
}

void CollectionReferenceInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kGetId, kGetPath, kGetParent, kDocumentAutoId,
                   kDocument, kAdd);
}

const std::string& CollectionReferenceInternal::id() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (cached_id_.empty()) {
    Env env = GetEnv();
    cached_id_ = env.Call(obj_, kGetId).ToString(env);
  }
  return cached_id_;
}

const std::string& CollectionReferenceInternal::path() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (cached_path_.empty()) {
    Env env = GetEnv();
    cached_path_ = env.Call(obj_, kGetPath).ToString(env);
  }
  return cached_path_;
}

DocumentReference CollectionReferenceInternal::Parent() const {
  Env env = GetEnv();
  Local<Object> parent = env.Call(obj_, kGetParent);
  return firestore_->NewDocumentReference(env, parent);
}

DocumentReference CollectionReferenceInternal::Document() const {
  Env env = GetEnv();
  Local<Object> document = env.Call(obj_, kDocumentAutoId);
  return firestore_->NewDocumentReference(env, document);
}

DocumentReference CollectionReferenceInternal::Document(
    const std::string& document_path) const {
  Env env = GetEnv();
  Local<String> java_path = env.NewStringUtf(document_path);
  Local<Object> document = env.Call(obj_, kDocument, java_path);
  return firestore_->NewDocumentReference(env, document);
}

Future<DocumentReference> CollectionReferenceInternal::Add(
    const MapFieldValue& data) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);
  Local<Task> task = env.Call(obj_, kAdd, java_data);
  return promises_.NewFuture<DocumentReference>(env, AsyncFn::kAdd, task);
}

}  // namespace firestore
}  // namespace firebase
