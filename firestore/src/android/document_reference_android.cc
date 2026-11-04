// Copyright 2020 Google LLC

#include "firestore/src/android/document_reference_android.h"

#include <utility>

#include "app/src/assert.h"
#include "firestore/src/android/collection_reference_android.h"
#include "firestore/src/android/event_listener_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/lambda_event_listener.h"
#include "firestore/src/android/listener_registration_android.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/android/set_options_android.h"
#include "firestore/src/android/source_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/task.h"

namespace firebase::firestore {
namespace {

using jni::Env;
using jni::HashMap;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::String;
using jni::Task;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/DocumentReference";
jclass clazz = nullptr;

Method<Object> kGetFirestore(
    "getFirestore", "()Lcom/google/firebase/firestore/FirebaseFirestore;");
Method<String> kGetId("getId", "()Ljava/lang/String;");
Method<String> kGetPath("getPath", "()Ljava/lang/String;");
Method<Object> kGetParent(
    "getParent", "()Lcom/google/firebase/firestore/CollectionReference;");
Method<Object> kCollection(
    "collection",
    "(Ljava/lang/String;)Lcom/google/firebase/firestore/CollectionReference;");
Method<Task> kGet(
    "get",
    "(Lcom/google/firebase/firestore/Source;)Lcom/google/android/gms/tasks/Task;");
Method<Task> kSet(
    "set",
    "(Ljava/lang/Object;Lcom/google/firebase/firestore/SetOptions;)"
    "Lcom/google/android/gms/tasks/Task;");
Method<Task> kUpdate("update",
                     "(Ljava/util/Map;)Lcom/google/android/gms/tasks/Task;");
Method<Task> kUpdateVarargs(
    "update",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;"
    "[Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;");
Method<Task> kDelete("delete", "()Lcom/google/android/gms/tasks/Task;");
Method<Object> kAddSnapshotListener(
    "addSnapshotListener",
    "(Ljava/util/concurrent/Executor;"
    "Lcom/google/firebase/firestore/MetadataChanges;"
    "Lcom/google/firebase/firestore/EventListener;)"
    "Lcom/google/firebase/firestore/ListenerRegistration;");

}  // namespace

void DocumentReferenceInternal::Initialize(jni::Loader& loader) {
  clazz = loader.LoadClass(kClassName, kGetFirestore, kGetId, kGetPath,
                           kGetParent, kCollection, kGet, kSet, kUpdate,
                           kUpdateVarargs, kDelete, kAddSnapshotListener);
}

DocumentReferenceInternal::DocumentReferenceInternal(
    const DocumentReferenceInternal& other)
    : Wrapper(other), promises_(other.promises_) {
  std::lock_guard<std::mutex> lock(other.mutex_);
  cached_id_ = other.cached_id_;
  cached_path_ = other.cached_path_;
}

DocumentReferenceInternal::DocumentReferenceInternal(
    DocumentReferenceInternal&& other) noexcept
    : Wrapper(std::move(other)), promises_(std::move(other.promises_)) {
  std::lock_guard<std::mutex> lock(other.mutex_);
  cached_id_ = std::move(other.cached_id_);
  cached_path_ = std::move(other.cached_path_);
}

DocumentReference DocumentReferenceInternal::Create(Env& env,
                                                    const Object& reference) {
  if (!env.ok() || !reference) return {};
  auto* firestore = FirestoreInternal::RecoverFirestore(
      env, env.Call(reference, kGetFirestore));
  return firestore ? firestore->NewDocumentReference(env, reference)
                   : DocumentReference{};
}

Firestore* DocumentReferenceInternal::firestore() const {
  return firestore_ ? firestore_->firestore_public() : nullptr;
}

const std::string& DocumentReferenceInternal::id() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (cached_id_.empty()) {
    Env env = GetEnv();
    cached_id_ = env.Call(obj_, kGetId).ToString(env);
  }
  return cached_id_;
}

const std::string& DocumentReferenceInternal::path() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (cached_path_.empty()) {
    Env env = GetEnv();
    cached_path_ = env.Call(obj_, kGetPath).ToString(env);
  }
  return cached_path_;
}

CollectionReference DocumentReferenceInternal::Parent() const {
  Env env = GetEnv();
  return firestore_->NewCollectionReference(env, env.Call(obj_, kGetParent));
}

CollectionReference DocumentReferenceInternal::Collection(
    const std::string& collection_path) const {
  Env env = GetEnv();
  return firestore_->NewCollectionReference(
      env, env.Call(obj_, kCollection, env.NewStringUtf(collection_path)));
}

Future<DocumentSnapshot> DocumentReferenceInternal::Get(Source source) {
  Env env = GetEnv();
  return promises_.NewFuture<DocumentSnapshot>(
      env, AsyncFn::kGet,
      env.Call(obj_, kGet, SourceInternal::Create(env, source)));
}

Future<void> DocumentReferenceInternal::Set(const MapFieldValue& data,
                                            const SetOptions& options) {
  Env env = GetEnv();
  return promises_.NewFuture<void>(
      env, AsyncFn::kSet,
      env.Call(obj_, kSet, MakeJavaMap(env, data),
               SetOptionsInternal::Create(env, options)));
}

Future<void> DocumentReferenceInternal::Update(const MapFieldValue& data) {
  Env env = GetEnv();
  return promises_.NewFuture<void>(
      env, AsyncFn::kUpdate, env.Call(obj_, kUpdate, MakeJavaMap(env, data)));
}

Future<void> DocumentReferenceInternal::Update(const MapFieldPathValue& data) {
  if (data.empty()) return Update(MapFieldValue{});
  Env env = GetEnv();
  UpdateFieldPathArgs args = MakeUpdateFieldPathArgs(env, data);
  return promises_.NewFuture<void>(
      env, AsyncFn::kUpdateFieldPath,
      env.Call(obj_, kUpdateVarargs, args.first_field, args.first_value,
               args.varargs));
}

Future<void> DocumentReferenceInternal::Delete() {
  Env env = GetEnv();
  return promises_.NewFuture<void>(env, AsyncFn::kDelete,
                                   env.Call(obj_, kDelete));
}

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const DocumentSnapshot&, Error, const std::string&)>
        callback) {
  return AddSnapshotListener(
      metadata_changes,
      std::make_unique<LambdaEventListener<DocumentSnapshot>>(
          std::move(callback)));
}

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    EventListener<DocumentSnapshot>* listener) {
  return AddSnapshotListenerInternal(metadata_changes, listener, false);
}

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::unique_ptr<EventListener<DocumentSnapshot>> listener) {
  return AddSnapshotListenerInternal(metadata_changes, listener.release(),
                                     true);
}

ListenerRegistration DocumentReferenceInternal::AddSnapshotListenerInternal(
    MetadataChanges metadata_changes,
    EventListener<DocumentSnapshot>* listener,
    bool passing_listener_ownership) {
  Env env = GetEnv();
  Local<Object> java_listener =
      EventListenerInternal::Create(env, firestore_, listener);
  Local<Object> java_registration = env.Call(
      obj_, kAddSnapshotListener, firestore_->user_callback_executor(),
      MetadataChangesInternal::Create(env, metadata_changes), java_listener);
  if (!env.ok() || !java_registration) {
    if (passing_listener_ownership) delete listener;
    return {};
  }
  return ListenerRegistration(new ListenerRegistrationInternal(
      firestore_, listener, passing_listener_ownership, java_registration,
      java_listener));
}

jni::Class DocumentReferenceInternal::GetClass() { return jni::Class(clazz); }

}  // namespace firebase::firestore
