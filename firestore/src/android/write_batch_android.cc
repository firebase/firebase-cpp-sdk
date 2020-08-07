#include "firestore/src/android/write_batch_android.h"

#include <jni.h>

#include <utility>

#include "app/src/util_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/set_options_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::HashMap;
using jni::Local;
using jni::Object;

}  // namespace

// clang-format off
#define WRITE_BATCH_METHODS(X)                                                 \
  X(Set, "set",                                                                \
    "(Lcom/google/firebase/firestore/DocumentReference;Ljava/lang/Object;"     \
    "Lcom/google/firebase/firestore/SetOptions;)"                              \
    "Lcom/google/firebase/firestore/WriteBatch;"),                             \
  X(Update, "update",                                                          \
    "(Lcom/google/firebase/firestore/DocumentReference;Ljava/util/Map;)"       \
    "Lcom/google/firebase/firestore/WriteBatch;"),                             \
  X(UpdateVarargs, "update",                                                   \
    "(Lcom/google/firebase/firestore/DocumentReference;"                       \
    "Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;"              \
    "[Ljava/lang/Object;)Lcom/google/firebase/firestore/WriteBatch;"),         \
  X(Delete, "delete", "(Lcom/google/firebase/firestore/DocumentReference;)"    \
    "Lcom/google/firebase/firestore/WriteBatch;"),                             \
  X(Commit, "commit", "()Lcom/google/android/gms/tasks/Task;")
// clang-format on

METHOD_LOOKUP_DECLARATION(write_batch, WRITE_BATCH_METHODS)
METHOD_LOOKUP_DEFINITION(write_batch,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/WriteBatch",
                         WRITE_BATCH_METHODS)

void WriteBatchInternal::Set(const DocumentReference& document,
                             const MapFieldValue& data,
                             const SetOptions& options) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);
  Local<Object> java_options = SetOptionsInternal::Create(env, options);

  // Returned value is just Java `this` and can be ignored.
  env.Call<Object>(obj_, write_batch::GetMethodId(write_batch::kSet),
                   document.internal_->ToJava(), java_data, java_options);
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldValue& data) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);

  // Returned value is just Java `this` and can be ignored.
  env.Call<Object>(obj_, write_batch::GetMethodId(write_batch::kUpdate),
                   document.internal_->ToJava(), java_data);
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldPathValue& data) {
  if (data.empty()) {
    Update(document, MapFieldValue{});
    return;
  }

  Env env = GetEnv();
  UpdateFieldPathArgs args = MakeUpdateFieldPathArgs(env, data);

  // Returned value is just Java `this` and can be ignored.
  env.Call<Object>(obj_, write_batch::GetMethodId(write_batch::kUpdateVarargs),
                   document.internal_->ToJava(), args.first_field,
                   args.first_value, args.varargs);
}

void WriteBatchInternal::Delete(const DocumentReference& document) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  env->CallObjectMethod(obj_, write_batch::GetMethodId(write_batch::kDelete),
                        document.internal_->java_object());
  CheckAndClearJniExceptions(env);
}

Future<void> WriteBatchInternal::Commit() {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, write_batch::GetMethodId(write_batch::kCommit));
  CheckAndClearJniExceptions(env);

  auto promise = promises_.MakePromise<void>();
  promise.RegisterForTask(WriteBatchFn::kCommit, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

/* static */
bool WriteBatchInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = write_batch::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void WriteBatchInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  write_batch::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
