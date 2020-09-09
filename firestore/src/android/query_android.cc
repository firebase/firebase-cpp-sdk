#include "firestore/src/android/query_android.h"

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "firestore/src/android/direction_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/event_listener_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/lambda_event_listener.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/android/source_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Object;

}  // namespace

METHOD_LOOKUP_DEFINITION(query,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/Query",
                         QUERY_METHODS)

Firestore* QueryInternal::firestore() {
  FIREBASE_ASSERT(firestore_->firestore_public() != nullptr);
  return firestore_->firestore_public();
}

Query QueryInternal::OrderBy(const FieldPath& field,
                             Query::Direction direction) {
  Env new_env = GetEnv();
  JNIEnv* env = new_env.get();
  Local<Object> j_field = FieldPathConverter::Create(new_env, field);
  CheckAndClearJniExceptions(env);
  Local<Object> j_direction = DirectionInternal::Create(new_env, direction);
  jobject query =
      env->CallObjectMethod(obj_, query::GetMethodId(query::kOrderBy),
                            j_field.get(), j_direction.get());
  CheckAndClearJniExceptions(env);
  QueryInternal* internal = new QueryInternal{firestore_, query};
  env->DeleteLocalRef(query);

  CheckAndClearJniExceptions(env);
  return Query{internal};
}

Query QueryInternal::Limit(int32_t limit) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  // Although the backend supports signed int32, Android client SDK uses long
  // as parameter type. So we cast it to jlong instead of jint here.
  jobject query = env->CallObjectMethod(obj_, query::GetMethodId(query::kLimit),
                                        static_cast<jlong>(limit));
  CheckAndClearJniExceptions(env);
  QueryInternal* internal = new QueryInternal{firestore_, query};
  env->DeleteLocalRef(query);

  CheckAndClearJniExceptions(env);
  return Query{internal};
}

Query QueryInternal::LimitToLast(int32_t limit) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  // Although the backend supports signed int32, Android client SDK uses long
  // as parameter type. So we cast it to jlong instead of jint here.
  jobject query = env->CallObjectMethod(
      obj_, query::GetMethodId(query::kLimitToLast), static_cast<jlong>(limit));
  CheckAndClearJniExceptions(env);
  QueryInternal* internal = new QueryInternal{firestore_, query};
  env->DeleteLocalRef(query);

  CheckAndClearJniExceptions(env);
  return Query{internal};
}

Future<QuerySnapshot> QueryInternal::Get(Source source) {
  Env new_env = GetEnv();
  Local<Object> java_source = SourceInternal::Create(new_env, source);

  JNIEnv* env = new_env.get();
  CheckAndClearJniExceptions(env);

  jobject task = env->CallObjectMethod(obj_, query::GetMethodId(query::kGet),
                                       java_source.get());
  CheckAndClearJniExceptions(env);

  auto promise = promises_.MakePromise<QuerySnapshot>();
  promise.RegisterForTask(QueryFn::kGet, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

/* static */
bool QueryInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = query::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void QueryInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  query::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

Query QueryInternal::Where(const FieldPath& field, query::Method method,
                           const FieldValue& value) {
  Env new_env = GetEnv();
  JNIEnv* env = new_env.get();
  Local<Object> path = FieldPathConverter::Create(new_env, field);
  CheckAndClearJniExceptions(env);
  jobject query =
      env->CallObjectMethod(obj_, query::GetMethodId(method), path.get(),
                            value.internal_->java_object());
  CheckAndClearJniExceptions(env);
  QueryInternal* internal = new QueryInternal{firestore_, query};
  env->DeleteLocalRef(query);

  CheckAndClearJniExceptions(env);
  return Query{internal};
}

Query QueryInternal::Where(const FieldPath& field, query::Method method,
                           const std::vector<FieldValue>& values) {
  Env new_env = GetEnv();
  JNIEnv* env = new_env.get();

  // Convert std::vector into java.util.List object.
  // TODO(chenbrian): Refactor this into a helper function.
  jobject converted_values = env->NewObject(
      util::array_list::GetClass(),
      util::array_list::GetMethodId(util::array_list::kConstructor));
  jmethodID add_method = util::array_list::GetMethodId(util::array_list::kAdd);
  jsize size = static_cast<jsize>(values.size());
  for (jsize i = 0; i < size; ++i) {
    // ArrayList.Add() always returns true, which we have no use for.
    env->CallBooleanMethod(converted_values, add_method,
                           values[i].internal_->java_object());
    CheckAndClearJniExceptions(env);
  }

  Local<Object> path = FieldPathConverter::Create(new_env, field);
  CheckAndClearJniExceptions(env);
  jobject query = env->CallObjectMethod(obj_, query::GetMethodId(method),
                                        path.get(), converted_values);
  CheckAndClearJniExceptions(env);
  QueryInternal* internal = new QueryInternal{firestore_, query};
  env->DeleteLocalRef(query);
  env->DeleteLocalRef(converted_values);

  return Query{internal};
}

Query QueryInternal::WithBound(query::Method method,
                               const DocumentSnapshot& snapshot) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject query = env->CallObjectMethod(obj_, query::GetMethodId(method),
                                        snapshot.internal_->java_object());
  CheckAndClearJniExceptions(env);
  QueryInternal* internal = new QueryInternal{firestore_, query};
  env->DeleteLocalRef(query);

  CheckAndClearJniExceptions(env);
  return Query{internal};
}

Query QueryInternal::WithBound(query::Method method,
                               const std::vector<FieldValue>& values) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  jobjectArray converted_values = ConvertFieldValues(env, values);
  jobject query =
      env->CallObjectMethod(obj_, query::GetMethodId(method), converted_values);
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(converted_values);
  QueryInternal* internal = new QueryInternal{firestore_, query};
  env->DeleteLocalRef(query);

  CheckAndClearJniExceptions(env);
  return Query{internal};
}

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
ListenerRegistration QueryInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const QuerySnapshot&, Error, const std::string&)>
        callback) {
  LambdaEventListener<QuerySnapshot>* listener =
      new LambdaEventListener<QuerySnapshot>(firebase::Move(callback));
  return AddSnapshotListener(metadata_changes, listener,
                             /*passing_listener_ownership=*/true);
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

ListenerRegistration QueryInternal::AddSnapshotListener(
    MetadataChanges metadata_changes, EventListener<QuerySnapshot>* listener,
    bool passing_listener_ownership) {
  Env new_env = GetEnv();
  JNIEnv* env = new_env.get();

  // Create listener.
  jobject java_listener =
      EventListenerInternal::EventListenerToJavaEventListener(env, firestore_,
                                                              listener);
  Local<Object> java_metadata =
      MetadataChangesInternal::Create(new_env, metadata_changes);
  CheckAndClearJniExceptions(env);

  // Register listener.
  jobject java_registration = env->CallObjectMethod(
      obj_, query::GetMethodId(query::kAddSnapshotListener),
      firestore_->user_callback_executor(), java_metadata.get(), java_listener);
  env->DeleteLocalRef(java_listener);
  CheckAndClearJniExceptions(env);

  // Wrapping
  ListenerRegistrationInternal* registration = new ListenerRegistrationInternal{
      firestore_, listener, passing_listener_ownership, java_registration};
  env->DeleteLocalRef(java_registration);

  return ListenerRegistration{registration};
}

jobjectArray QueryInternal::ConvertFieldValues(
    JNIEnv* env, const std::vector<FieldValue>& field_values) {
  jsize size = static_cast<jsize>(field_values.size());
  jobjectArray result = env->NewObjectArray(size, util::object::GetClass(),
                                            /*initialElement=*/nullptr);
  for (jsize i = 0; i < size; ++i) {
    env->SetObjectArrayElement(result, i,
                               field_values[i].internal_->java_object());
  }

  return result;
}

bool operator==(const QueryInternal& lhs, const QueryInternal& rhs) {
  return lhs.EqualsJavaObject(rhs);
}

}  // namespace firestore
}  // namespace firebase
