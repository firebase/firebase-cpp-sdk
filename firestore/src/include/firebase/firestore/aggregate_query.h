/*
 * Copyright 2023 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_QUERY_H_
#define FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_QUERY_H_

#include "firebase/firestore/aggregate_source.h"

#include <cstddef>

namespace firebase {
/// @cond FIREBASE_APP_INTERNAL
template <typename T>
class Future;
/// @endcond

namespace firestore {

class AggregateQueryInternal;
class AggregateQuerySnapshot;
class Query;

/**
 * @brief A query that calculates aggregations over an underlying query.
 */
class AggregateQuery {
 public:
  /**
   * @brief Creates an invalid AggregateQuery that has to be reassigned before
   * it can be used.
   *
   * Calling any member function on an invalid AggregateQuery will be a no-op.
   * If the function returns a value, it will return a zero, empty, or invalid
   * value, depending on the type of the value.
   */
  AggregateQuery();

  /**
   * @brief Copy constructor.
   *
   * `AggregateQuery` is immutable and can be efficiently copied (no deep copy
   * is performed).
   *
   * @param[in] other `AggregateQuery` to copy from.
   */
  AggregateQuery(const AggregateQuery& other);

  /**
   * @brief Move constructor.
   *
   * Moving is more efficient than copying for a `AggregateQuery`. After being
   * moved from, a `AggregateQuery` is equivalent to its default-constructed
   * state.
   *
   * @param[in] other `AggregateQuery` to move data from.
   */
  AggregateQuery(AggregateQuery&& other);

  virtual ~AggregateQuery();

  /**
   * @brief Copy assignment operator.
   *
   * `AggregateQuery` is immutable and can be efficiently copied (no deep copy
   * is performed).
   *
   * @param[in] other `AggregateQuery` to copy from.
   *
   * @return Reference to the destination `AggregateQuery`.
   */
  AggregateQuery& operator=(const AggregateQuery& other);

  /**
   * @brief Move assignment operator.
   *
   * Moving is more efficient than copying for a `AggregateQuery`. After being
   * moved from, a `AggregateQuery` is equivalent to its default-constructed
   * state.
   *
   * @param[in] other `AggregateQuery` to move data from.
   *
   * @return Reference to the destination `AggregateQuery`.
   */
  AggregateQuery& operator=(AggregateQuery&& other);

  /**
   * @brief Returns the query whose aggregations will be calculated by this
   * object.
   */
  virtual Query query() const;

  /**
   * @brief Executes this query.
   *
   * @param[in] aggregate_source The source from which to acquire the aggregate
   * results.
   *
   * @return A Future that will be resolved with the results of the
   * AggregateQuery.
   */
  virtual Future<AggregateQuerySnapshot> Get(
      AggregateSource aggregate_source) const;

  /**
   * @brief Returns true if this `AggregateQuery` is valid, false if it is not
   * valid. An invalid `AggregateQuery` could be the result of:
   *   - Creating a `AggregateQuery` using the default constructor.
   *   - Moving from the `AggregateQuery`.
   *   - Deleting your Firestore instance, which will invalidate all the
   *     `AggregateQuery` instances associated with it.
   *
   * @return true if this `AggregateQuery` is valid, false if this
   * `AggregateQuery` is invalid.
   */
  bool is_valid() const { return internal_ != nullptr; }

 private:
  std::size_t Hash() const;

  friend class AggregateQueryInternal;
  friend struct ConverterImpl;

  friend bool operator==(const AggregateQuery& lhs, const AggregateQuery& rhs);
  friend std::size_t AggregateQueryHash(const AggregateQuery& aggregate_query);

  template <typename T, typename U, typename F>
  friend struct CleanupFn;

  explicit AggregateQuery(AggregateQueryInternal* internal);

  mutable AggregateQueryInternal* internal_ = nullptr;
};

/** Checks `lhs` and `rhs` for equality. */
bool operator==(const AggregateQuery& lhs, const AggregateQuery& rhs);

/** Checks `lhs` and `rhs` for inequality. */
inline bool operator!=(const AggregateQuery& lhs, const AggregateQuery& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_QUERY_H_
