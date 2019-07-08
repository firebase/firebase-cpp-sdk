// Copyright 2016 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_QUERY_SPEC_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_QUERY_SPEC_H_

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"

namespace firebase {
namespace database {
class Database;

namespace internal {

// QueryParams are the set of filters and sorting options to apply to a Query.
struct QueryParams {
  QueryParams() : order_by(kOrderByPriority), limit_first(0), limit_last(0) {}

  // Compare two QueryParams, which are considered the same if all fields are
  // the same (except only compare order_by_child if order_by is kOrderByChild).
  bool operator==(const QueryParams& other) const {
    return order_by == other.order_by &&
           (order_by != kOrderByChild ||
            order_by_child == other.order_by_child) &&
           start_at_value == other.start_at_value &&
           start_at_child_key == other.start_at_child_key &&
           end_at_value == other.end_at_value &&
           end_at_child_key == other.end_at_child_key &&
           equal_to_value == other.equal_to_value &&
           equal_to_child_key == other.equal_to_child_key &&
           limit_first == other.limit_first && limit_last == other.limit_last;
  }

  // Less-than operator, required so we can place QuerySpec instances in an
  // std::map. This is an arbitrary operation, but must be consistent.
  bool operator<(const QueryParams& other) const {
    if (order_by < other.order_by) return true;
    if (order_by > other.order_by) return false;
    if (order_by == kOrderByChild && other.order_by == kOrderByChild) {
      if (order_by_child < other.order_by_child) return true;
      if (order_by_child > other.order_by_child) return false;
    }
    if (start_at_value < other.start_at_value) return true;
    if (start_at_value > other.start_at_value) return false;
    if (start_at_child_key < other.start_at_child_key) return true;
    if (start_at_child_key > other.start_at_child_key) return false;
    if (end_at_value < other.end_at_value) return true;
    if (end_at_value > other.end_at_value) return false;
    if (end_at_child_key < other.end_at_child_key) return true;
    if (end_at_child_key > other.end_at_child_key) return false;
    if (equal_to_value < other.equal_to_value) return true;
    if (equal_to_value > other.equal_to_value) return false;
    if (equal_to_child_key < other.equal_to_child_key) return true;
    if (equal_to_child_key > other.equal_to_child_key) return false;
    if (limit_first < other.limit_first) return true;
    if (limit_first > other.limit_first) return false;
    if (limit_last < other.limit_last) return true;
    if (limit_last > other.limit_last) return false;
    return false;
  }

  enum OrderBy { kOrderByPriority, kOrderByChild, kOrderByKey, kOrderByValue };

  // Set by Query::OrderByPriority(), Query::OrderByChild(),
  // Query::OrderByKey(), and Query::OrderByValue().
  // Default is kOrderByPriority.
  OrderBy order_by;

  // Set by Query::OrderByChild(). Only valid if order_by is kOrderByChild.
  std::string order_by_child;

  // Set by Query::StartAt(). Variant::Null() if unspecified.
  Variant start_at_value;
  // Set by Query::StartAt() with child specified. Blank if unspecified.
  std::string start_at_child_key;

  // Set by Query::EndAt(). Variant::Null() if unspecified.
  Variant end_at_value;
  // Set by Query::EndAt() with child specified. Blank if unspecified.
  std::string end_at_child_key;

  // Set by Query::EqualTo(). Variant::Null() if unspecified.
  Variant equal_to_value;
  // Set by Query::EqualTo() with child specified. Blank if unspecified.
  std::string equal_to_child_key;

  // Set by Query::LimitToFirst(). 0 means no limit.
  size_t limit_first;
  // Set by Query::LimitToLast(). 0 means no limit.
  size_t limit_last;
};

// Query specifier. When you add a Listener to a query, the Listener is indexed
// not by the Query itself, but by the Query's QuerySpec. This allows you to
// remove a listener from a different (but matching) Query to the original.
struct QuerySpec {
  QuerySpec() : path(), params() {}
  explicit QuerySpec(const Path& _path) : path(_path), params() {}
  explicit QuerySpec(const QueryParams& _params) : path(), params(_params) {}
  QuerySpec(const Path& _path, const QueryParams& _params)
      : path(_path), params(_params) {}

  // Compare two QuerySpecs, which are considered the same if all fields are the
  // same.
  bool operator==(const QuerySpec& other) const {
    return path == other.path && params == other.params;
  }

  // Less-than operator, required so we can place QuerySpec instances in an
  // std::map. This is an arbitrary operation, but must be consistent.
  bool operator<(const QuerySpec& other) const {
    if (path < other.path) return true;
    if (path > other.path) return false;
    if (params < other.params) return true;
    return false;
  }

  // Full path this query refers to. Only changes when a DatabaseReference is
  // created.
  Path path;

  // Parameters that define how a query is being filtered.
  QueryParams params;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_QUERY_SPEC_H_
