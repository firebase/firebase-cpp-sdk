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

#ifndef FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_QUERY_SNAPSHOT_H_
#define FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_QUERY_SNAPSHOT_H_

#include <cstddef>
#include <cstdint>

namespace firebase {
namespace firestore {

class AggregateQuery;
class AggregateQuerySnapshotInternal;

/**
 * @brief The results of executing an AggregateQuery.
 *
 * @note Firestore classes are not meant to be subclassed except for use in test
 * mocks. Subclassing is not supported in production code and new SDK releases
 * may break code that does so.
 */
class AggregateQuerySnapshot {
 public:
  /**
   * @brief Creates an invalid AggregateQuerySnapshot that has to be reassigned
   * before it can be used.
   *
   * Calling any member function on an invalid AggregateQuerySnapshot will be a
   * no-op. If the function returns a value, it will return a zero, empty, or
   * invalid value, depending on the type of the value.
   */
  AggregateQuerySnapshot();

  /**
   * @brief Copy constructor.
   *
   * `AggregateQuerySnapshot` is immutable and can be efficiently copied (no
   * deep copy is performed).
   *
   * @param[in] other `AggregateQuerySnapshot` to copy from.
   */
  AggregateQuerySnapshot(const AggregateQuerySnapshot& other);

  /**
   * @brief Move constructor.
   *
   * Moving is more efficient than copying for a `AggregateQuerySnapshot`. After
   * being moved from, a `AggregateQuerySnapshot` is equivalent to its
   * default-constructed state.
   *
   * @param[in] other `AggregateQuerySnapshot` to move data from.
   */
  AggregateQuerySnapshot(AggregateQuerySnapshot&& other);

  virtual ~AggregateQuerySnapshot();

  /**
   * @brief Copy assignment operator.
   *
   * `AggregateQuerySnapshot` is immutable and can be efficiently copied (no
   * deep copy is performed).
   *
   * @param[in] other `AggregateQuerySnapshot` to copy from.
   *
   * @return Reference to the destination `AggregateQuerySnapshot`.
   */
  AggregateQuerySnapshot& operator=(const AggregateQuerySnapshot& other);

  /**
   * @brief Move assignment operator.
   *
   * Moving is more efficient than copying for a `AggregateQuerySnapshot`. After
   * being moved from, a `AggregateQuerySnapshot` is equivalent to its
   * default-constructed state.
   *
   * @param[in] other `AggregateQuerySnapshot` to move data from.
   *
   * @return Reference to the destination `AggregateQuerySnapshot`.
   */
  AggregateQuerySnapshot& operator=(AggregateQuerySnapshot&& other);

  /**
   * @brief Returns the query that was executed to produce this result.
   *
   * @return The `AggregateQuery` instance.
   */
  virtual AggregateQuery query() const;

  /**
   * @brief Returns the number of documents in the result set of the underlying
   * query.
   *
   * @return The number of documents in the result set of the underlying query.
   */
  virtual int64_t count() const;

  /**
   * @brief Returns true if this `AggregateQuerySnapshot` is valid, false if it
   * is not valid. An invalid `AggregateQuerySnapshot` could be the result of:
   *   - Creating a `AggregateQuerySnapshot` using the default constructor.
   *   - Moving from the `AggregateQuerySnapshot`.
   *   - Deleting your Firestore instance, which will invalidate all the
   *     `AggregateQuerySnapshot` instances associated with it.
   *
   * @return true if this `AggregateQuerySnapshot` is valid, false if this
   * `AggregateQuerySnapshot` is invalid.
   */
  bool is_valid() const { return internal_ != nullptr; }

 private:
  std::size_t Hash() const;

  friend bool operator==(const AggregateQuerySnapshot& lhs,
                         const AggregateQuerySnapshot& rhs);
#ifndef SWIG
  friend std::size_t AggregateQuerySnapshotHash(
      const AggregateQuerySnapshot& snapshot);
#endif  // not SWIG
  friend struct ConverterImpl;
  friend class AggregateQuerySnapshotTest;

  template <typename T, typename U, typename F>
  friend struct CleanupFn;

  explicit AggregateQuerySnapshot(AggregateQuerySnapshotInternal* internal);

  mutable AggregateQuerySnapshotInternal* internal_ = nullptr;
};

/** Checks `lhs` and `rhs` for equality. */
bool operator==(const AggregateQuerySnapshot& lhs,
                const AggregateQuerySnapshot& rhs);

/** Checks `lhs` and `rhs` for inequality. */
inline bool operator!=(const AggregateQuerySnapshot& lhs,
                       const AggregateQuerySnapshot& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_QUERY_SNAPSHOT_H_
