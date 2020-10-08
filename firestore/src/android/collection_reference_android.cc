#include "firestore/src/android/collection_reference_android.h"

#include <string>
#include <utility>

#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/promise_android.h"
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
Method<Object> kAdd("add",
                    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;");

}  // namespace

void CollectionReferenceInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kGetId, kGetPath, kGetParent, kDocumentAutoId,
                   kDocument, kAdd);
}

const std::string& CollectionReferenceInternal::id() const {
  if (cached_id_.empty()) {
    Env env = GetEnv();
    cached_id_ = env.Call(obj_, kGetId).ToString(env);
  }
  return cached_id_;
}

const std::string& CollectionReferenceInternal::path() const {
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
  FieldValueInternal map_value(data);

  Env env = GetEnv();
  Local<Object> task = env.Call(obj_, kAdd, map_value.ToJava());
  return promises_.NewFuture<DocumentReference>(env, AsyncFn::kAdd, task);
}

}  // namespace firestore
}  // namespace firebase
