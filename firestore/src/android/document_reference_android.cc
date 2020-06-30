#include "firestore/src/android/document_reference_android.h"

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/util_android.h"
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

namespace firebase {
namespace firestore {

// clang-format off
#define DOCUMENT_REFERENCE_METHODS(X)                                 \
  X(GetId, "getId", "()Ljava/lang/String;"),                          \
  X(GetPath, "getPath", "()Ljava/lang/String;"),                      \
  X(GetParent, "getParent",                                           \
    "()Lcom/google/firebase/firestore/CollectionReference;"),         \
  X(Collection, "collection", "(Ljava/lang/String;)"                  \
    "Lcom/google/firebase/firestore/CollectionReference;"),           \
  X(Get, "get",                                                       \
    "(Lcom/google/firebase/firestore/Source;)"                        \
    "Lcom/google/android/gms/tasks/Task;"),                           \
  X(Set, "set",                                                       \
   "(Ljava/lang/Object;Lcom/google/firebase/firestore/SetOptions;)"   \
   "Lcom/google/android/gms/tasks/Task;"),                            \
  X(Update, "update",                                                 \
   "(Ljava/util/Map;)Lcom/google/android/gms/tasks/Task;"),           \
  X(UpdateVarargs, "update",                                          \
   "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;"     \
   "[Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;"),        \
  X(Delete, "delete", "()Lcom/google/android/gms/tasks/Task;"),       \
  X(AddSnapshotListener, "addSnapshotListener",                       \
    "(Ljava/util/concurrent/Executor;"                                \
    "Lcom/google/firebase/firestore/MetadataChanges;"                 \
    "Lcom/google/firebase/firestore/EventListener;)"                  \
    "Lcom/google/firebase/firestore/ListenerRegistration;")
// clang-format on

METHOD_LOOKUP_DECLARATION(document_reference, DOCUMENT_REFERENCE_METHODS)
METHOD_LOOKUP_DEFINITION(document_reference,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/DocumentReference",
                         DOCUMENT_REFERENCE_METHODS)

Firestore* DocumentReferenceInternal::firestore() {
  FIREBASE_ASSERT(firestore_->firestore_public() != nullptr);
  return firestore_->firestore_public();
}

const std::string& DocumentReferenceInternal::id() const {
  if (!cached_id_.empty()) {
    return cached_id_;
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jstring id = static_cast<jstring>(env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kGetId)));
  cached_id_ = util::JniStringToString(env, id);
  CheckAndClearJniExceptions(env);

  return cached_id_;
}

const std::string& DocumentReferenceInternal::path() const {
  if (!cached_path_.empty()) {
    return cached_path_;
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jstring path = static_cast<jstring>(env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kGetPath)));
  cached_path_ = util::JniStringToString(env, path);
  CheckAndClearJniExceptions(env);

  return cached_path_;
}

CollectionReference DocumentReferenceInternal::Parent() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject parent = env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kGetParent));
  CollectionReferenceInternal* internal =
      new CollectionReferenceInternal{firestore_, parent};
  env->DeleteLocalRef(parent);
  CheckAndClearJniExceptions(env);
  return CollectionReference(internal);
}

CollectionReference DocumentReferenceInternal::Collection(
    const std::string& collection_path) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(collection_path.c_str());
  jobject collection = env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kCollection),
      path_string);
  env->DeleteLocalRef(path_string);
  CheckAndClearJniExceptions(env);
  CollectionReferenceInternal* internal =
      new CollectionReferenceInternal{firestore_, collection};
  env->DeleteLocalRef(collection);
  CheckAndClearJniExceptions(env);
  return CollectionReference(internal);
}

Future<DocumentSnapshot> DocumentReferenceInternal::Get(Source source) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kGet),
      SourceInternal::ToJavaObject(env, source));
  CheckAndClearJniExceptions(env);

  auto promise = MakePromise<DocumentSnapshot>();
  promise.RegisterForTask(DocumentReferenceFn::kGet, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<DocumentSnapshot> DocumentReferenceInternal::GetLastResult() {
  return LastResult<DocumentSnapshot>(DocumentReferenceFn::kGet);
}

Future<void> DocumentReferenceInternal::Set(const MapFieldValue& data,
                                            const SetOptions& options) {
  FieldValueInternal map_value(data);
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject java_options = SetOptionsInternal::ToJavaObject(env, options);
  CheckAndClearJniExceptions(env);
  jobject task = env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kSet),
      map_value.java_object(), java_options);
  env->DeleteLocalRef(java_options);
  CheckAndClearJniExceptions(env);

  auto promise = MakePromise<void>();
  promise.RegisterForTask(DocumentReferenceFn::kSet, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<void> DocumentReferenceInternal::SetLastResult() {
  return LastResult<void>(DocumentReferenceFn::kSet);
}

Future<void> DocumentReferenceInternal::Update(const MapFieldValue& data) {
  FieldValueInternal map_value(data);
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kUpdate),
      map_value.java_object());
  CheckAndClearJniExceptions(env);

  auto promise = MakePromise<void>();
  promise.RegisterForTask(DocumentReferenceFn::kUpdate, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<void> DocumentReferenceInternal::Update(const MapFieldPathValue& data) {
  if (data.empty()) {
    return Update(MapFieldValue{});
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  auto iter = data.begin();
  jobject first_field = FieldPathConverter::ToJavaObject(env, iter->first);
  jobject first_value = iter->second.internal_->java_object();
  ++iter;

  // Make the varargs
  jobjectArray more_fields_and_values =
      MapFieldPathValueToJavaArray(firestore_, iter, data.end());

  jobject task = env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kUpdateVarargs),
      first_field, first_value, more_fields_and_values);
  env->DeleteLocalRef(first_field);
  env->DeleteLocalRef(more_fields_and_values);
  CheckAndClearJniExceptions(env);

  auto promise = MakePromise<void>();
  promise.RegisterForTask(DocumentReferenceFn::kUpdate, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<void> DocumentReferenceInternal::UpdateLastResult() {
  return LastResult<void>(DocumentReferenceFn::kUpdate);
}

Future<void> DocumentReferenceInternal::Delete() {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, document_reference::GetMethodId(document_reference::kDelete));
  CheckAndClearJniExceptions(env);

  auto promise = MakePromise<void>();
  promise.RegisterForTask(DocumentReferenceFn::kDelete, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<void> DocumentReferenceInternal::DeleteLastResult() {
  return LastResult<void>(DocumentReferenceFn::kDelete);
}

#if defined(FIREBASE_USE_STD_FUNCTION)

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const DocumentSnapshot&, Error)> callback) {
  LambdaEventListener<DocumentSnapshot>* listener =
      new LambdaEventListener<DocumentSnapshot>(firebase::Move(callback));
  return AddSnapshotListener(metadata_changes, listener,
                             /*passing_listener_ownership=*/true);
}

#endif  // defined(FIREBASE_USE_STD_FUNCTION)

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes, EventListener<DocumentSnapshot>* listener,
    bool passing_listener_ownership) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Create listener.
  jobject java_listener =
      EventListenerInternal::EventListenerToJavaEventListener(env, firestore_,
                                                              listener);
  jobject java_metadata =
      MetadataChangesInternal::ToJavaObject(env, metadata_changes);

  // Register listener.
  jobject java_registration = env->CallObjectMethod(
      obj_,
      document_reference::GetMethodId(document_reference::kAddSnapshotListener),
      firestore_->user_callback_executor(), java_metadata, java_listener);
  env->DeleteLocalRef(java_listener);
  CheckAndClearJniExceptions(env);

  // Wrapping
  ListenerRegistrationInternal* registration = new ListenerRegistrationInternal{
      firestore_, listener, passing_listener_ownership, java_registration};
  env->DeleteLocalRef(java_registration);

  return ListenerRegistration{registration};
}

/* static */
jclass DocumentReferenceInternal::GetClass() {
  return document_reference::GetClass();
}

/* static */
bool DocumentReferenceInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = document_reference::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void DocumentReferenceInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  document_reference::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
