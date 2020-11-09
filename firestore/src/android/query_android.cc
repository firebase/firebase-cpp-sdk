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
#include "firestore/src/android/listener_registration_android.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/android/source_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/array_list.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Array;
using jni::ArrayList;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/Query";
Method<Object> kEqualTo(
    "whereEqualTo",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kNotEqualTo(
    "whereNotEqualTo",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kLessThan(
    "whereLessThan",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kLessThanOrEqualTo(
    "whereLessThanOrEqualTo",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kGreaterThan(
    "whereGreaterThan",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kGreaterThanOrEqualTo(
    "whereGreaterThanOrEqualTo",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kArrayContains(
    "whereArrayContains",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kArrayContainsAny(
    "whereArrayContainsAny",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/util/List;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kIn("whereIn",
                   "(Lcom/google/firebase/firestore/FieldPath;Ljava/util/List;)"
                   "Lcom/google/firebase/firestore/Query;");
Method<Object> kNotIn(
    "whereNotIn",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/util/List;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kOrderBy("orderBy",
                        "(Lcom/google/firebase/firestore/FieldPath;"
                        "Lcom/google/firebase/firestore/Query$Direction;)"
                        "Lcom/google/firebase/firestore/Query;");
Method<Object> kLimit("limit", "(J)Lcom/google/firebase/firestore/Query;");
Method<Object> kLimitToLast("limitToLast",
                            "(J)Lcom/google/firebase/firestore/Query;");
Method<Object> kStartAtSnapshot(
    "startAt",
    "(Lcom/google/firebase/firestore/DocumentSnapshot;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kStartAt(
    "startAt", "([Ljava/lang/Object;)Lcom/google/firebase/firestore/Query;");
Method<Object> kStartAfterSnapshot(
    "startAfter",
    "(Lcom/google/firebase/firestore/DocumentSnapshot;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kStartAfter(
    "startAfter", "([Ljava/lang/Object;)Lcom/google/firebase/firestore/Query;");
Method<Object> kEndBeforeSnapshot(
    "endBefore",
    "(Lcom/google/firebase/firestore/DocumentSnapshot;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kEndBefore(
    "endBefore", "([Ljava/lang/Object;)Lcom/google/firebase/firestore/Query;");
Method<Object> kEndAtSnapshot(
    "endAt",
    "(Lcom/google/firebase/firestore/DocumentSnapshot;)"
    "Lcom/google/firebase/firestore/Query;");
Method<Object> kEndAt(
    "endAt", "([Ljava/lang/Object;)Lcom/google/firebase/firestore/Query;");
Method<Object> kGet("get",
                    "(Lcom/google/firebase/firestore/Source;)"
                    "Lcom/google/android/gms/tasks/Task;");
Method<Object> kAddSnapshotListener(
    "addSnapshotListener",
    "(Ljava/util/concurrent/Executor;"
    "Lcom/google/firebase/firestore/MetadataChanges;"
    "Lcom/google/firebase/firestore/EventListener;)"
    "Lcom/google/firebase/firestore/ListenerRegistration;");

}  // namespace

void QueryInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(
      kClassName, kEqualTo, kNotEqualTo, kLessThan, kLessThanOrEqualTo,
      kGreaterThan, kGreaterThanOrEqualTo, kArrayContains, kArrayContainsAny,
      kIn, kNotIn, kOrderBy, kLimit, kLimitToLast, kStartAtSnapshot, kStartAt,
      kStartAfterSnapshot, kStartAfter, kEndBeforeSnapshot, kEndBefore,
      kEndAtSnapshot, kEndAt, kGet, kAddSnapshotListener);
}

Firestore* QueryInternal::firestore() {
  FIREBASE_ASSERT(firestore_->firestore_public() != nullptr);
  return firestore_->firestore_public();
}

Query QueryInternal::WhereEqualTo(const FieldPath& field,
                                  const FieldValue& value) const {
  return Where(field, kEqualTo, value);
}

Query QueryInternal::WhereNotEqualTo(const FieldPath& field,
                                     const FieldValue& value) const {
  return Where(field, kNotEqualTo, value);
}

Query QueryInternal::WhereLessThan(const FieldPath& field,
                                   const FieldValue& value) const {
  return Where(field, kLessThan, value);
}

Query QueryInternal::WhereLessThanOrEqualTo(const FieldPath& field,
                                            const FieldValue& value) const {
  return Where(field, kLessThanOrEqualTo, value);
}

Query QueryInternal::WhereGreaterThan(const FieldPath& field,
                                      const FieldValue& value) const {
  return Where(field, kGreaterThan, value);
}

Query QueryInternal::WhereGreaterThanOrEqualTo(const FieldPath& field,
                                               const FieldValue& value) const {
  return Where(field, kGreaterThanOrEqualTo, value);
}

Query QueryInternal::WhereArrayContains(const FieldPath& field,
                                        const FieldValue& value) const {
  return Where(field, kArrayContains, value);
}

Query QueryInternal::WhereArrayContainsAny(
    const FieldPath& field, const std::vector<FieldValue>& values) const {
  return Where(field, kArrayContainsAny, values);
}

Query QueryInternal::WhereIn(const FieldPath& field,
                             const std::vector<FieldValue>& values) const {
  return Where(field, kIn, values);
}

Query QueryInternal::WhereNotIn(const FieldPath& field,
                                const std::vector<FieldValue>& values) const {
  return Where(field, kNotIn, values);
}

Query QueryInternal::OrderBy(const FieldPath& field,
                             Query::Direction direction) const {
  Env env = GetEnv();
  Local<Object> java_field = FieldPathConverter::Create(env, field);
  Local<Object> java_direction = DirectionInternal::Create(env, direction);
  Local<Object> query =
      env.Call(obj_, kOrderBy, java_field.get(), java_direction.get());
  return firestore_->NewQuery(env, query);
}

Query QueryInternal::Limit(int32_t limit) const {
  Env env = GetEnv();

  // Although the backend only supports int32, the Android client SDK uses long
  // as parameter type.
  auto java_limit = static_cast<jlong>(limit);
  Local<Object> query = env.Call(obj_, kLimit, java_limit);
  return firestore_->NewQuery(env, query);
}

Query QueryInternal::LimitToLast(int32_t limit) const {
  Env env = GetEnv();

  // Although the backend only supports int32, the Android client SDK uses long
  // as parameter type.
  auto java_limit = static_cast<jlong>(limit);
  Local<Object> query = env.Call(obj_, kLimitToLast, java_limit);
  return firestore_->NewQuery(env, query);
}

Query QueryInternal::StartAt(const DocumentSnapshot& snapshot) const {
  return WithBound(kStartAtSnapshot, snapshot);
}

Query QueryInternal::StartAt(const std::vector<FieldValue>& values) const {
  return WithBound(kStartAt, values);
}

Query QueryInternal::StartAfter(const DocumentSnapshot& snapshot) const {
  return WithBound(kStartAfterSnapshot, snapshot);
}

Query QueryInternal::StartAfter(const std::vector<FieldValue>& values) const {
  return WithBound(kStartAfter, values);
}

Query QueryInternal::EndBefore(const DocumentSnapshot& snapshot) const {
  return WithBound(kEndBeforeSnapshot, snapshot);
}

Query QueryInternal::EndBefore(const std::vector<FieldValue>& values) const {
  return WithBound(kEndBefore, values);
}

Query QueryInternal::EndAt(const DocumentSnapshot& snapshot) const {
  return WithBound(kEndAtSnapshot, snapshot);
}

Query QueryInternal::EndAt(const std::vector<FieldValue>& values) const {
  return WithBound(kEndAt, values);
}

Future<QuerySnapshot> QueryInternal::Get(Source source) {
  Env env = GetEnv();
  Local<Object> java_source = SourceInternal::Create(env, source);
  Local<Object> task = env.Call(obj_, kGet, java_source);
  return promises_.NewFuture<QuerySnapshot>(env, AsyncFn::kGet, task);
}

Query QueryInternal::Where(const FieldPath& field, const Method<Object>& method,
                           const FieldValue& value) const {
  Env env = GetEnv();
  Local<Object> java_field = FieldPathConverter::Create(env, field);
  Local<Object> query = env.Call(obj_, method, java_field, ToJava(value));
  return firestore_->NewQuery(env, query);
}

Query QueryInternal::Where(const FieldPath& field, const Method<Object>& method,
                           const std::vector<FieldValue>& values) const {
  Env env = GetEnv();

  size_t size = values.size();
  Local<ArrayList> java_values = ArrayList::Create(env, size);
  for (size_t i = 0; i < size; ++i) {
    java_values.Add(env, ToJava(values[i]));
  }

  Local<Object> java_field = FieldPathConverter::Create(env, field);
  Local<Object> query = env.Call(obj_, method, java_field, java_values);
  return firestore_->NewQuery(env, query);
}

Query QueryInternal::WithBound(const Method<Object>& method,
                               const DocumentSnapshot& snapshot) const {
  Env env = GetEnv();
  Local<Object> query = env.Call(obj_, method, snapshot.internal_->ToJava());
  return firestore_->NewQuery(env, query);
}

Query QueryInternal::WithBound(const Method<Object>& method,
                               const std::vector<FieldValue>& values) const {
  Env env = GetEnv();
  Local<Array<Object>> java_values = ConvertFieldValues(env, values);
  Local<Object> query = env.Call(obj_, method, java_values);
  return firestore_->NewQuery(env, query);
}

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
ListenerRegistration QueryInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const QuerySnapshot&, Error, const std::string&)>
        callback) {
  auto* listener =
      new LambdaEventListener<QuerySnapshot>(firebase::Move(callback));
  return AddSnapshotListener(metadata_changes, listener,
                             /*passing_listener_ownership=*/true);
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

ListenerRegistration QueryInternal::AddSnapshotListener(
    MetadataChanges metadata_changes, EventListener<QuerySnapshot>* listener,
    bool passing_listener_ownership) {
  Env env = GetEnv();

  Local<Object> java_listener =
      EventListenerInternal::Create(env, firestore_, listener);
  Local<Object> java_metadata =
      MetadataChangesInternal::Create(env, metadata_changes);

  Local<Object> java_registration =
      env.Call(obj_, kAddSnapshotListener, firestore_->user_callback_executor(),
               java_metadata, java_listener);

  if (!env.ok()) return {};
  return ListenerRegistration(new ListenerRegistrationInternal(
      firestore_, listener, passing_listener_ownership, java_registration));
}

Local<Array<Object>> QueryInternal::ConvertFieldValues(
    Env& env, const std::vector<FieldValue>& field_values) const {
  size_t size = field_values.size();
  Local<Array<Object>> result = env.NewArray(size, Object::GetClass());
  for (size_t i = 0; i < size; ++i) {
    result.Set(env, i, ToJava(field_values[i]));
  }
  return result;
}

bool operator==(const QueryInternal& lhs, const QueryInternal& rhs) {
  Env env = FirestoreInternal::GetEnv();
  return Object::Equals(env, lhs.ToJava(), rhs.ToJava());
}

}  // namespace firestore
}  // namespace firebase
