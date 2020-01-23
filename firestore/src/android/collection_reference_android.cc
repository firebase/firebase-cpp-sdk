#include "firestore/src/android/collection_reference_android.h"


#include <string>
#include <utility>

#include "app/src/util_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define COLLECTION_REFERENCE_METHODS(X)                               \
  X(GetId, "getId", "()Ljava/lang/String;"),                          \
  X(GetPath, "getPath", "()Ljava/lang/String;"),                      \
  X(GetParent, "getParent",                                           \
    "()Lcom/google/firebase/firestore/DocumentReference;"),           \
  X(DocumentAutoId, "document",                                       \
    "()Lcom/google/firebase/firestore/DocumentReference;"),           \
  X(Document, "document", "(Ljava/lang/String;)"                      \
    "Lcom/google/firebase/firestore/DocumentReference;"),             \
  X(Add, "add",                                                       \
    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;")
// clang-format on

METHOD_LOOKUP_DECLARATION(collection_reference, COLLECTION_REFERENCE_METHODS)
METHOD_LOOKUP_DEFINITION(collection_reference,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/CollectionReference",
                         COLLECTION_REFERENCE_METHODS)

const std::string& CollectionReferenceInternal::id() const {
  if (!cached_id_.empty()) {
    return cached_id_;
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jstring id = static_cast<jstring>(env->CallObjectMethod(
      obj_, collection_reference::GetMethodId(collection_reference::kGetId)));
  cached_id_ = util::JniStringToString(env, id);
  CheckAndClearJniExceptions(env);

  return cached_id_;
}

const std::string& CollectionReferenceInternal::path() const {
  if (!cached_path_.empty()) {
    return cached_path_;
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jstring path = static_cast<jstring>(env->CallObjectMethod(
      obj_, collection_reference::GetMethodId(collection_reference::kGetPath)));
  cached_path_ = util::JniStringToString(env, path);
  CheckAndClearJniExceptions(env);

  return cached_path_;
}

DocumentReference CollectionReferenceInternal::Parent() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject parent = env->CallObjectMethod(
      obj_,
      collection_reference::GetMethodId(collection_reference::kGetParent));
  CheckAndClearJniExceptions(env);
  if (parent == nullptr) {
    return DocumentReference();
  } else {
    DocumentReferenceInternal* internal =
        new DocumentReferenceInternal{firestore_, parent};
    env->DeleteLocalRef(parent);
    return DocumentReference(internal);
  }
}

DocumentReference CollectionReferenceInternal::Document() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject document = env->CallObjectMethod(
      obj_,
      collection_reference::GetMethodId(collection_reference::kDocumentAutoId));
  DocumentReferenceInternal* internal =
      new DocumentReferenceInternal{firestore_, document};
  env->DeleteLocalRef(document);
  CheckAndClearJniExceptions(env);
  return DocumentReference(internal);
}

DocumentReference CollectionReferenceInternal::Document(
    const std::string& document_path) const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(document_path.c_str());
  jobject document = env->CallObjectMethod(
      obj_, collection_reference::GetMethodId(collection_reference::kDocument),
      path_string);
  env->DeleteLocalRef(path_string);
  CheckAndClearJniExceptions(env);
  DocumentReferenceInternal* internal =
      new DocumentReferenceInternal{firestore_, document};
  env->DeleteLocalRef(document);
  CheckAndClearJniExceptions(env);
  return DocumentReference(internal);
}

Future<DocumentReference> CollectionReferenceInternal::Add(
    const MapFieldValue& data) {
  FieldValueInternal map_value(data);
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, collection_reference::GetMethodId(collection_reference::kAdd),
      map_value.java_object());
  CheckAndClearJniExceptions(env);

  auto promise = MakePromise<DocumentReference, DocumentReferenceInternal>();
  promise.RegisterForTask(CollectionReferenceFn::kAdd, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<DocumentReference> CollectionReferenceInternal::AddLastResult() {
  return LastResult<DocumentReference>(CollectionReferenceFn::kAdd);
}

/* static */
bool CollectionReferenceInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = collection_reference::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void CollectionReferenceInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  collection_reference::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
