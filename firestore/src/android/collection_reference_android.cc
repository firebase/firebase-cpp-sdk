// Copyright 2020 Google LLC

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

namespace firebase::firestore {
namespace {

using jni::Env;
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
Method<Object> kDocument(
    "document",
    "(Ljava/lang/String;)Lcom/google/firebase/firestore/DocumentReference;");
Method<Task> kAdd("add",
                  "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;");

}  // namespace

CollectionReferenceInternal::CollectionReferenceInternal(
    const CollectionReferenceInternal& other)
    : QueryInternal(other) {
  std::scoped_lock lock(other.mutex_);
  cached_id_ = other.cached_id_;
  cached_path_ = other.cached_path_;
}

CollectionReferenceInternal::CollectionReferenceInternal(
    CollectionReferenceInternal&& other) noexcept
    : QueryInternal(std::move(other)) {
  std::scoped_lock lock(other.mutex_);
  cached_id_ = std::move(other.cached_id_);
  cached_path_ = std::move(other.cached_path_);
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
  return firestore_->NewDocumentReference(env, env.Call(obj_, kGetParent));
}

DocumentReference CollectionReferenceInternal::Document() const {
  Env env = GetEnv();
  return firestore_->NewDocumentReference(env, env.Call(obj_, kDocumentAutoId));
}

DocumentReference CollectionReferenceInternal::Document(
    const std::string& document_path) const {
  Env env = GetEnv();
  return firestore_->NewDocumentReference(
      env, env.Call(obj_, kDocument, env.NewStringUtf(document_path)));
}

Future<DocumentReference> CollectionReferenceInternal::Add(
    const MapFieldValue& data) {
  Env env = GetEnv();
  return promises_.NewFuture<DocumentReference>(
      env, AsyncFn::kAdd, env.Call(obj_, kAdd, MakeJavaMap(env, data)));
}

}  // namespace firebase::firestore
