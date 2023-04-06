/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_QUERY_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_QUERY_ANDROID_H_

#include <cstdint>

#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/android/promise_factory_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/query.h"

namespace firebase {
namespace firestore {

class Firestore;

class QueryInternal : public Wrapper {
 public:
  // Each API of Query that returns a Future needs to define an enum value here.
  // For example, a Future-returning method Foo() relies on the enum value kFoo.
  // The enum values are used to identify and manage Future in the Firestore
  // Future manager.
  enum class AsyncFn {
    // Enum values for the baseclass Query.
    kGet = 0,

    // Enum values below are for the subclass CollectionReference.
    kAdd,

    // Must be the last enum value.
    kCount,
  };

  static void Initialize(jni::Loader& loader);

  QueryInternal(FirestoreInternal* firestore, const jni::Object& object)
      : Wrapper(firestore, object), promises_(firestore) {}

  /** Gets the Firestore instance associated with this query. */
  Firestore* firestore();

  /**
   * @brief Returns a query that counts the documents in the result set of this
   * query.
   *
   * The returned query, when executed, counts the documents in the result set
   * of this query without actually downloading the documents.
   *
   * Using the returned query to count the documents is efficient because only
   * the final count, not the documents' data, is downloaded. The returned query
   * can even count the documents if the result set would be prohibitively large
   * to download entirely (e.g. thousands of documents).
   *
   * @return A query that counts the documents in the result set of this query.
   */
  virtual AggregateQuery Count() const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field and the value should be equal to
   * the specified value.
   *
   * @param[in] field The path of the field to compare
   * @param[in] value The value for comparison
   *
   * @return The created Query.
   */
  Query WhereEqualTo(const FieldPath& field, const FieldValue& value) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field and the value does not equal the
   * specified value.
   *
   * A Query can have only one `WhereNotEqualTo()` filter, and it cannot be
   * combined with `WhereNotIn()`.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] value The value for comparison.
   *
   * @return The created Query.
   */
  Query WhereNotEqualTo(const FieldPath& field, const FieldValue& value) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field and the value should be less
   * than the specified value.
   *
   * @param[in] field The path of the field to compare
   * @param[in] value The value for comparison
   *
   * @return The created Query.
   */
  Query WhereLessThan(const FieldPath& field, const FieldValue& value) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field and the value should be less
   * than or equal to the specified value.
   *
   * @param[in] field The path of the field to compare
   * @param[in] value The value for comparison
   *
   * @return The created Query.
   */
  Query WhereLessThanOrEqualTo(const FieldPath& field,
                               const FieldValue& value) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field and the value should be greater
   * than the specified value.
   *
   * @param[in] field The path of the field to compare
   * @param[in] value The value for comparison
   *
   * @return The created Query.
   */
  Query WhereGreaterThan(const FieldPath& field, const FieldValue& value) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field and the value should be greater
   * than or equal to the specified value.
   *
   * @param[in] field The path of the field to compare
   * @param[in] value The value for comparison
   *
   * @return The created Query.
   */
  Query WhereGreaterThanOrEqualTo(const FieldPath& field,
                                  const FieldValue& value) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field, the value must be an array, and
   * that the array must contain the provided value.
   *
   * A Query can have only one `WhereArrayContains()` filter.
   *
   * @param[in] field The path of the field containing an array to search
   * @param[in] value The value that must be contained in the array
   *
   * @return The created Query.
   */
  Query WhereArrayContains(const FieldPath& field,
                           const FieldValue& value) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field, the value must be an array, and
   * that the array must contain at least one value from the provided list.
   *
   * A Query can have only one `WhereArrayContainsAny()` filter and it cannot be
   * combined with `WhereArrayContains()` or `WhereIn()`.
   *
   * @param[in] field The path of the field containing an array to search.
   * @param[in] values The list that contains the values to match.
   *
   * @return The created Query.
   */
  Query WhereArrayContainsAny(const FieldPath& field,
                              const std::vector<FieldValue>& values) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field and the value must equal one of
   * the values from the provided list.
   *
   * A Query can have only one `WhereIn()` filter and it cannot be
   * combined with `WhereArrayContainsAny()`.
   *
   * @param[in] field The path of the field containing an array to search.
   * @param[in] values The list that contains the values to match.
   *
   * @return The created Query.
   */
  Query WhereIn(const FieldPath& field,
                const std::vector<FieldValue>& values) const;

  /**
   * @brief Creates and returns a new Query with the additional filter that
   * documents must contain the specified field and the value must not equal any
   * of the values from the provided list.
   *
   * One special case is that `WhereNotIn` cannot match `FieldValue::Null()`
   * values. To query for documents where a field exists and is
   * `FieldValue::Null()`, use `WhereNotEqualTo`, which can handle this special
   * case.
   *
   * A `Query` can have only one `WhereNotIn()` filter, and it cannot be
   * combined with `WhereArrayContains()`, `WhereArrayContainsAny()`,
   * `WhereIn()`, or `WhereNotEqualTo()`.
   *
   * @param[in] field The name of the field containing an array to search.
   * @param[in] values The list that contains the values to match.
   *
   * @return The created Query.
   */
  Query WhereNotIn(const FieldPath& field,
                   const std::vector<FieldValue>& values) const;

  /**
   * @brief Creates and returns a new Query that's additionally sorted by the
   * specified field.
   *
   * @param[in] field The field to sort by.
   * @param[in] direction The direction to sort.
   *
   * @return The created Query.
   */
  Query OrderBy(const FieldPath& field, Query::Direction direction) const;

  /**
   * @brief Creates and returns a new Query that only returns the first matching
   * documents up to the specified number.
   *
   * @param[in] limit A non-negative integer to specify the maximum number of
   * items to return.
   *
   * @return The created Query.
   */
  virtual Query Limit(int32_t limit) const;

  /**
   * @brief Creates and returns a new Query that only returns the last matching
   * documents up to the specified number.
   *
   * @param[in] limit A non-negative integer to specify the maximum number of
   * items to return.
   *
   * @return The created Query.
   */
  virtual Query LimitToLast(int32_t limit) const;

  /**
   * @brief Creates and returns a new Query that starts at the provided document
   * (inclusive). The starting position is relative to the order of the query.
   * The document must contain all of the fields provided in the OrderBy of this
   * query.
   *
   * @param[in] snapshot The snapshot of the document to start at.
   *
   * @return The created Query.
   */
  Query StartAt(const DocumentSnapshot& snapshot) const;

  /**
   * @brief Creates and returns a new Query that starts at the provided fields
   * relative to the order of the query. The order of the field values must
   * match the order of the order by clauses of the query.
   *
   * @param[in] values The field values to start this query at, in order of the
   * query's order by.
   *
   * @return The created Query.
   */
  Query StartAt(const std::vector<FieldValue>& values) const;

  /**
   * @brief Creates and returns a new Query that starts after the provided
   * document (inclusive). The starting position is relative to the order of the
   * query. The document must contain all of the fields provided in the OrderBy
   * of this query.
   *
   * @param[in] snapshot The snapshot of the document to start after.
   *
   * @return The created Query.
   */
  Query StartAfter(const DocumentSnapshot& snapshot) const;

  /**
   * @brief Creates and returns a new Query that starts after the provided
   * fields relative to the order of the query. The order of the field values
   * must match the order of the order by clauses of the query.
   *
   * @param[in] values The field values to start this query after, in order of
   * the query's order by.
   *
   * @return The created Query.
   */
  Query StartAfter(const std::vector<FieldValue>& values) const;

  /**
   * @brief Creates and returns a new Query that ends before the provided
   * document (inclusive). The end position is relative to the order of the
   * query. The document must contain all of the fields provided in the OrderBy
   * of this query.
   *
   * @param[in] snapshot The snapshot of the document to end before.
   *
   * @return The created Query.
   */
  Query EndBefore(const DocumentSnapshot& snapshot) const;

  /**
   * @brief Creates and returns a new Query that ends before the provided fields
   * relative to the order of the query. The order of the field values must
   * match the order of the order by clauses of the query.
   *
   * @param[in] values The field values to end this query before, in order of
   * the query's order by.
   *
   * @return The created Query.
   */
  Query EndBefore(const std::vector<FieldValue>& values) const;

  /**
   * @brief Creates and returns a new Query that ends at the provided document
   * (inclusive). The end position is relative to the order of the query. The
   * document must contain all of the fields provided in the OrderBy of this
   * query.
   *
   * @param[in] snapshot The snapshot of the document to end at.
   *
   * @return The created Query.
   */
  Query EndAt(const DocumentSnapshot& snapshot) const;

  /**
   * @brief Creates and returns a new Query that ends at the provided fields
   * relative to the order of the query. The order of the field values must
   * match the order of the order by clauses of the query.
   *
   * @param[in] values The field values to end this query at, in order of the
   * query's order by.
   *
   * @return The created Query.
   */
  Query EndAt(const std::vector<FieldValue>& values) const;

  /**
   * @brief Executes the query and returns the results as a QuerySnapshot.
   *
   * By default, Get() attempts to provide up-to-date data when possible by
   * waiting for data from the server, but it may return cached data or fail if
   * you are offline and the server cannot be reached. This behavior can be
   * altered via the {@link Source} parameter.
   *
   * @param[in] source A value to configure the get behavior.
   *
   * @return A Future that will be resolved with the results of the Query.
   */
  virtual Future<QuerySnapshot> Get(Source source);

  /**
   * @brief Starts listening to the QuerySnapshot events referenced by this
   * query.
   *
   * @param[in] metadata_changes Indicates whether metadata-only changes (i.e.
   * only QuerySnapshot.getMetadata() changed) should trigger snapshot events.
   * @param[in] When this function is called, snapshot value is valid if and
   * only if error is Error::kOk.
   *
   * @return A registration object that can be used to remove the listener.
   */
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::function<void(const QuerySnapshot&, Error, const std::string&)>
          callback);

  /**
   * @brief Starts listening to the QuerySnapshot events referenced by this
   * query.
   *
   * @param[in] metadata_changes Indicates whether metadata-only changes (i.e.
   * only QuerySnapshot.getMetadata() changed) should trigger snapshot events.
   * @param[in] listener The event listener that will be called with the
   * snapshots, which must remain in memory until you remove the listener from
   * this Query. (Ownership is not transferred; you are responsible for making
   * sure that listener is valid as long as this Query is valid and the listener
   * is registered.)
   *
   * @return A registration object that can be used to remove the listener.
   */
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      EventListener<QuerySnapshot>* listener,
      bool passing_listener_ownership = false);

  size_t Hash() const;

 protected:
  PromiseFactory<AsyncFn> promises_;

 private:
  friend class FirestoreInternal;

  // A generalized function for all WhereFoo calls.
  Query Where(const FieldPath& field,
              const jni::Method<jni::Object>& method,
              const FieldValue& value) const;
  Query Where(const FieldPath& field,
              const jni::Method<jni::Object>& method,
              const std::vector<FieldValue>& values) const;

  // A generalized function for all {Start|End}{Before|After|At} calls.
  Query WithBound(const jni::Method<jni::Object>& method,
                  const DocumentSnapshot& snapshot) const;
  Query WithBound(const jni::Method<jni::Object>& method,
                  const std::vector<FieldValue>& values) const;

  // A helper function to convert std::vector<FieldValue> to Java FieldValue[].
  jni::Local<jni::Array<jni::Object>> ConvertFieldValues(
      jni::Env& env, const std::vector<FieldValue>& field_values) const;
};

bool operator==(const QueryInternal& lhs, const QueryInternal& rhs);
inline bool operator!=(const QueryInternal& lhs, const QueryInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_QUERY_ANDROID_H_
