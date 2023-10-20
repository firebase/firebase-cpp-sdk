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

#ifndef FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_FILTER_H_
#define FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_FILTER_H_

#include <string>
#include <vector>

#include "firebase/firestore/field_value.h"

namespace firebase {
namespace firestore {

class FilterInternal;

/**
 * @brief A Filter represents a restriction on one or more field values and can
 * be used to refine the results of a Query.
 */
class Filter {
 public:
  /**
   * @brief Creates a new filter for checking that the given array field
   * contains the given value.
   *
   * @param[in] field The name of the field containing an array to search.
   * @param[in] value The value that must be contained in the array.
   *
   * @return The newly created filter.
   */
  static Filter ArrayContains(const std::string& field,
                              const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given array field
   * contains any of the given values.
   *
   * @param[in] field The name of the field containing an array to search.
   * @param[in] values The list of values to match.
   *
   * @return The newly created filter.
   */
  static Filter ArrayContainsAny(const std::string& field,
                                 const std::vector<FieldValue>& values);

  /**
   * @brief Creates a new filter for checking that the given field is equal to
   * the given value.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter EqualTo(const std::string& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is not equal
   * to the given value.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter NotEqualTo(const std::string& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is greater
   * than the given value.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter GreaterThan(const std::string& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is greater
   * than or equal to the given value.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter GreaterThanOrEqualTo(const std::string& field,
                                     const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is less than
   * the given value.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter LessThan(const std::string& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is less than
   * or equal to the given value.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter LessThanOrEqualTo(const std::string& field,
                                  const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field equals any of
   * the given values.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] values The list of values to match.
   *
   * @return The newly created filter.
   */
  static Filter In(const std::string& field,
                   const std::vector<FieldValue>& values);

  /**
   * @brief Creates a new filter for checking that the given field does not
   * equal any of the given values.
   *
   * @param[in] field The name of the field to compare.
   * @param[in] values The list of values to match.
   *
   * @return The newly created filter.
   */
  static Filter NotIn(const std::string& field,
                      const std::vector<FieldValue>& values);

  /**
   * @brief Creates a new filter for checking that the given array field
   * contains the given value.
   *
   * @param[in] field The path of the field containing an array to search.
   * @param[in] value The value that must be contained in the array.
   *
   * @return The newly created filter.
   */
  static Filter ArrayContains(const FieldPath& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given array field
   * contains any of the given values.
   *
   * @param[in] field The path of the field containing an array to search.
   * @param[in] values The list of values to match.
   *
   * @return The newly created filter.
   */
  static Filter ArrayContainsAny(const FieldPath& field,
                                 const std::vector<FieldValue>& values);

  /**
   * @brief Creates a new filter for checking that the given field is equal to
   * the given value.
   *
   * @param[in] field The path of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter EqualTo(const FieldPath& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is not equal
   * to the given value.
   *
   * @param[in] field The path of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter NotEqualTo(const FieldPath& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is greater
   * than the given value.
   *
   * @param[in] field The path of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter GreaterThan(const FieldPath& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is greater
   * than or equal to the given value.
   *
   * @param[in] field The path of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter GreaterThanOrEqualTo(const FieldPath& field,
                                     const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is less than
   * the given value.
   *
   * @param[in] field The path of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter LessThan(const FieldPath& field, const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field is less than
   * or equal to the given value.
   *
   * @param[in] field The path of the field to compare.
   * @param[in] value The value for comparison
   *
   * @return The newly created filter.
   */
  static Filter LessThanOrEqualTo(const FieldPath& field,
                                  const FieldValue& value);

  /**
   * @brief Creates a new filter for checking that the given field equals any of
   * the given values.
   *
   * @param[in] field The path of the field to compare.
   * @param[in] values The list of values to match.
   *
   * @return The newly created filter.
   */
  static Filter In(const FieldPath& field,
                   const std::vector<FieldValue>& values);

  /**
   * @brief Creates a new filter for checking that the given field does not
   * equal any of the given values.
   *
   * @param[in] field The path of the field to compare.
   * @param[in] values The list of values to match.
   *
   * @return The newly created filter.
   */
  static Filter NotIn(const FieldPath& field,
                      const std::vector<FieldValue>& values);

  /**
   * @brief Creates a new filter that is a conjunction of the given filters. A
   * conjunction filter includes a document if it satisfies all of the given
   * filters.
   *
   * If no filter is given, the composite filter is a no-op, and if only one
   * filter is given, the composite filter has the same behavior as the
   * underlying filter.
   *
   * @param[in] filters The filters to perform a conjunction for.
   *
   * @return The newly created filter.
   */
  template <typename... Filters>
  static Filter And(const Filters&... filters) {
    return AndInternal(filters...);
  }

  /**
   * @brief Creates a new filter that is a conjunction of the given filters. A
   * conjunction filter includes a document if it satisfies all of the given
   * filters.
   *
   * If no filter is given, the composite filter is a no-op, and if only one
   * filter is given, the composite filter has the same behavior as the
   * underlying filter.
   *
   * @param[in] filters The list that contains filters to perform a conjunction
   * for.
   *
   * @return The newly created filter.
   */
  static Filter And(const std::vector<Filter>& filters);

  /**
   * @brief Creates a new filter that is a disjunction of the given filters. A
   * disjunction filter includes a document if it satisfies <em>any</em> of the
   * given filters.
   *
   * If no filter is given, the composite filter is a no-op, and if only one
   * filter is given, the composite filter has the same behavior as the
   * underlying filter.
   *
   * @param[in] filters The filters to perform a disjunction for.
   *
   * @return The newly created filter.
   */
  template <typename... Filters>
  static Filter Or(const Filters&... filters) {
    return OrInternal(filters...);
  }

  /**
   * @brief Creates a new filter that is a disjunction of the given filters. A
   * disjunction filter includes a document if it satisfies <em>any</em> of the
   * given filters.
   *
   * If no filter is given, the composite filter is a no-op, and if only one
   * filter is given, the composite filter has the same behavior as the
   * underlying filter.
   *
   * @param[in] filters The list that contains filters to perform a disjunction
   * for.
   *
   * @return The newly created filter.
   */
  static Filter Or(const std::vector<Filter>& filters);

  /**
   * @brief Copy constructor.
   *
   * `Filter` is immutable and can be efficiently copied.
   *
   * @param[in] other `Filter` to copy from.
   */
  Filter(const Filter& other);

  /**
   * @brief Move constructor.
   *
   * @param[in] other `Filter` to move data from.
   */
  Filter(Filter&& other) noexcept;

  /**
   * @brief Copy assignment operator.
   *
   * `Filter` is immutable and can be efficiently copied.
   *
   * @param[in] other `Filter` to copy from.
   *
   * @return Reference to the destination `Filter`.
   */
  Filter& operator=(const Filter& other);

  /**
   * @brief Move assignment operator.
   *
   * @param[in] other `Filter` to move data from.
   *
   * @return Reference to the destination `Filter`.
   */
  Filter& operator=(Filter&& other) noexcept;

  ~Filter();

 private:
  friend class Query;
  friend class QueryInternal;
  friend class FilterInternal;
  friend bool operator==(const Filter& lhs, const Filter& rhs);
  friend struct ConverterImpl;

  static inline Filter AndInternal(const Filter& filter) { return filter; }

  template <typename... Filters>
  static inline Filter AndInternal(const Filters&... filters) {
    return And(std::vector<Filter>({filters...}));
  }

  static inline Filter OrInternal(const Filter& filter) { return filter; }

  template <typename... Filters>
  static inline Filter OrInternal(const Filters&... filters) {
    return Or(std::vector<Filter>({filters...}));
  }

  bool IsEmpty() const;

  explicit Filter(FilterInternal* internal);
  FilterInternal* internal_ = nullptr;
};

/** Checks `lhs` and `rhs` for equality. */
bool operator==(const Filter& lhs, const Filter& rhs);

/** Checks `lhs` and `rhs` for inequality. */
inline bool operator!=(const Filter& lhs, const Filter& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_FILTER_H_
