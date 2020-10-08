#include "firestore/src/android/write_batch_android.h"

#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/set_options_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::HashMap;
using jni::Local;
using jni::Method;
using jni::Object;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/WriteBatch";
Method<Object> kSet(
    "set",
    "(Lcom/google/firebase/firestore/DocumentReference;Ljava/lang/Object;"
    "Lcom/google/firebase/firestore/SetOptions;)"
    "Lcom/google/firebase/firestore/WriteBatch;");
Method<Object> kUpdate(
    "update",
    "(Lcom/google/firebase/firestore/DocumentReference;Ljava/util/Map;)"
    "Lcom/google/firebase/firestore/WriteBatch;");
Method<Object> kUpdateVarargs(
    "update",
    "(Lcom/google/firebase/firestore/DocumentReference;"
    "Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;"
    "[Ljava/lang/Object;)Lcom/google/firebase/firestore/WriteBatch;");
Method<Object> kDelete("delete",
                       "(Lcom/google/firebase/firestore/DocumentReference;)"
                       "Lcom/google/firebase/firestore/WriteBatch;");
Method<Object> kCommit("commit", "()Lcom/google/android/gms/tasks/Task;");

}  // namespace

void WriteBatchInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kSet, kUpdate, kUpdateVarargs, kDelete, kCommit);
}

void WriteBatchInternal::Set(const DocumentReference& document,
                             const MapFieldValue& data,
                             const SetOptions& options) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);
  Local<Object> java_options = SetOptionsInternal::Create(env, options);

  env.Call(obj_, kSet, ToJava(document), java_data, java_options);
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldValue& data) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);

  env.Call(obj_, kUpdate, ToJava(document), java_data);
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldPathValue& data) {
  if (data.empty()) {
    Update(document, MapFieldValue{});
    return;
  }

  Env env = GetEnv();
  UpdateFieldPathArgs args = MakeUpdateFieldPathArgs(env, data);

  env.Call(obj_, kUpdateVarargs, ToJava(document), args.first_field,
           args.first_value, args.varargs);
}

void WriteBatchInternal::Delete(const DocumentReference& document) {
  Env env = GetEnv();
  env.Call(obj_, kDelete, ToJava(document));
}

Future<void> WriteBatchInternal::Commit() {
  Env env = GetEnv();
  Local<Object> task = env.Call(obj_, kCommit);
  return promises_.NewFuture<void>(env, AsyncFn::kCommit, task);
}

jni::Object WriteBatchInternal::ToJava(const DocumentReference& reference) {
  return reference.internal_ ? reference.internal_->ToJava() : jni::Object();
}

}  // namespace firestore
}  // namespace firebase
