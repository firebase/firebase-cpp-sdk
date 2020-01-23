#include "firestore/src/android/write_batch_android.h"

#include <jni.h>

#include <utility>

#include "app/src/util_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/set_options_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

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
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  jobject data_map = MapFieldValueToJavaMap(firestore_, data);
  jobject java_options = SetOptionsInternal::ToJavaObject(env, options);
  env->CallObjectMethod(obj_, write_batch::GetMethodId(write_batch::kSet),
                        document.internal_->java_object(), data_map,
                        java_options);
  env->DeleteLocalRef(data_map);
  env->DeleteLocalRef(java_options);
  CheckAndClearJniExceptions(env);
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldValue& data) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  jobject data_map = MapFieldValueToJavaMap(firestore_, data);
  env->CallObjectMethod(obj_, write_batch::GetMethodId(write_batch::kUpdate),
                        document.internal_->java_object(), data_map);
  env->DeleteLocalRef(data_map);
  CheckAndClearJniExceptions(env);
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldPathValue& data) {
  if (data.empty()) {
    Update(document, MapFieldValue{});
    return;
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  auto iter = data.begin();
  jobject first_field = FieldPathConverter::ToJavaObject(env, iter->first);
  jobject first_value = iter->second.internal_->java_object();
  ++iter;

  // Make the varargs
  jobjectArray more_fields_and_values =
      MapFieldPathValueToJavaArray(firestore_, iter, data.end());

  env->CallObjectMethod(obj_,
                        write_batch::GetMethodId(write_batch::kUpdateVarargs),
                        document.internal_->java_object(), first_field,
                        first_value, more_fields_and_values);
  env->DeleteLocalRef(first_field);
  env->DeleteLocalRef(more_fields_and_values);
  CheckAndClearJniExceptions(env);
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

  auto promise = MakePromise<void, void>();
  promise.RegisterForTask(WriteBatchFn::kCommit, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<void> WriteBatchInternal::CommitLastResult() {
  return LastResult<void>(WriteBatchFn::kCommit);
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
