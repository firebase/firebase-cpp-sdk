#include "firestore/src/android/document_reference_android.h"

#include "app/meta/move.h"
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
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Class;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::String;

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
    "(Ljava/lang/String;)"
    "Lcom/google/firebase/firestore/CollectionReference;");
Method<Object> kGet("get",
                    "(Lcom/google/firebase/firestore/Source;)"
                    "Lcom/google/android/gms/tasks/Task;");
Method<Object> kSet(
    "set",
    "(Ljava/lang/Object;Lcom/google/firebase/firestore/SetOptions;)"
    "Lcom/google/android/gms/tasks/Task;");
Method<Object> kUpdate("update",
                       "(Ljava/util/Map;)Lcom/google/android/gms/tasks/Task;");
Method<Object> kUpdateVarargs(
    "update",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;"
    "[Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;");
Method<Object> kDelete("delete", "()Lcom/google/android/gms/tasks/Task;");
Method<Object> kAddSnapshotListener(
    "addSnapshotListener",
    "(Ljava/util/concurrent/Executor;"
    "Lcom/google/firebase/firestore/MetadataChanges;"
    "Lcom/google/firebase/firestore/EventListener;)"
    "Lcom/google/firebase/firestore/ListenerRegistration;");

}  // namespace

void DocumentReferenceInternal::Initialize(jni::Loader& loader) {
  clazz = loader.LoadClass(kClassName);
  loader.LoadAll(kGetFirestore, kGetId, kGetPath, kGetParent, kCollection, kGet,
                 kSet, kUpdate, kUpdateVarargs, kDelete, kAddSnapshotListener);
}

DocumentReference DocumentReferenceInternal::Create(Env& env,
                                                    const Object& reference) {
  if (!reference) return {};

  Local<Object> java_firestore = env.Call(reference, kGetFirestore);
  auto* firestore = FirestoreInternal::RecoverFirestore(env, java_firestore);
  if (firestore == nullptr) return {};

  return firestore->NewDocumentReference(env, reference);
}

Firestore* DocumentReferenceInternal::firestore() {
  FIREBASE_ASSERT(firestore_->firestore_public() != nullptr);
  return firestore_->firestore_public();
}

const std::string& DocumentReferenceInternal::id() const {
  if (cached_id_.empty()) {
    Env env = GetEnv();
    cached_id_ = env.Call(obj_, kGetId).ToString(env);
  }
  return cached_id_;
}

const std::string& DocumentReferenceInternal::path() const {
  if (cached_path_.empty()) {
    Env env = GetEnv();
    cached_path_ = env.Call(obj_, kGetPath).ToString(env);
  }
  return cached_path_;
}

CollectionReference DocumentReferenceInternal::Parent() const {
  Env env = GetEnv();
  Local<Object> parent = env.Call(obj_, kGetParent);
  return firestore_->NewCollectionReference(env, parent);
}

CollectionReference DocumentReferenceInternal::Collection(
    const std::string& collection_path) {
  Env env = GetEnv();
  Local<String> java_path = env.NewStringUtf(collection_path);
  Local<Object> collection = env.Call(obj_, kCollection, java_path);
  return firestore_->NewCollectionReference(env, collection);
}

Future<DocumentSnapshot> DocumentReferenceInternal::Get(Source source) {
  Env env = GetEnv();
  Local<Object> java_source = SourceInternal::Create(env, source);
  Local<Object> task = env.Call(obj_, kGet, java_source);
  return promises_.NewFuture<DocumentSnapshot>(env, AsyncFn::kGet, task);
}

Future<void> DocumentReferenceInternal::Set(const MapFieldValue& data,
                                            const SetOptions& options) {
  Env env = GetEnv();
  FieldValueInternal map_value(data);
  Local<Object> java_options = SetOptionsInternal::Create(env, options);
  Local<Object> task = env.Call(obj_, kSet, map_value, java_options);
  return promises_.NewFuture<void>(env, AsyncFn::kSet, task);
}

Future<void> DocumentReferenceInternal::Update(const MapFieldValue& data) {
  Env env = GetEnv();
  FieldValueInternal map_value(data);
  Local<Object> task = env.Call(obj_, kUpdate, map_value);
  return promises_.NewFuture<void>(env, AsyncFn::kUpdate, task);
}

Future<void> DocumentReferenceInternal::Update(const MapFieldPathValue& data) {
  if (data.empty()) {
    return Update(MapFieldValue{});
  }

  Env env = GetEnv();
  UpdateFieldPathArgs args = MakeUpdateFieldPathArgs(env, data);
  Local<Object> task = env.Call(obj_, kUpdateVarargs, args.first_field,
                                args.first_value, args.varargs);

  return promises_.NewFuture<void>(env, AsyncFn::kUpdate, task);
}

Future<void> DocumentReferenceInternal::Delete() {
  Env env = GetEnv();
  Local<Object> task = env.Call(obj_, kDelete);
  return promises_.NewFuture<void>(env, AsyncFn::kDelete, task);
}

#if defined(FIREBASE_USE_STD_FUNCTION)

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const DocumentSnapshot&, Error, const std::string&)>
        callback) {
  LambdaEventListener<DocumentSnapshot>* listener =
      new LambdaEventListener<DocumentSnapshot>(firebase::Move(callback));
  return AddSnapshotListener(metadata_changes, listener,
                             /*passing_listener_ownership=*/true);
}

#endif  // defined(FIREBASE_USE_STD_FUNCTION)

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes, EventListener<DocumentSnapshot>* listener,
    bool passing_listener_ownership) {
  Env env = GetEnv();
  Local<Object> java_metadata =
      MetadataChangesInternal::Create(env, metadata_changes);
  Local<Object> java_listener =
      EventListenerInternal::Create(env, firestore_, listener);

  Local<Object> java_registration =
      env.Call(obj_, kAddSnapshotListener, firestore_->user_callback_executor(),
               java_metadata, java_listener);

  if (!env.ok() || !java_registration) return {};
  return ListenerRegistration(new ListenerRegistrationInternal(
      firestore_, listener, passing_listener_ownership, java_registration));
}

Class DocumentReferenceInternal::GetClass() { return Class(clazz); }

}  // namespace firestore
}  // namespace firebase
