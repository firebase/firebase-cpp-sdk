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

#ifndef FIREBASE_DATABASE_SRC_COMMON_QUERY_SPEC_H_
#define FIREBASE_DATABASE_SRC_COMMON_QUERY_SPEC_H_

#include <optional>
#include <string>
#include <tuple>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"

namespace firebase {
namespace database {
class Database;

namespace internal {

struct QueryParams {
  enum OrderBy { kOrderByPriority, kOrderByChild, kOrderByKey, kOrderByValue };

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

  bool operator<(const QueryParams& other) const {
    const std::string empty;
    const std::string& lhs_child =
        order_by == kOrderByChild ? order_by_child : empty;
    const std::string& rhs_child =
        other.order_by == kOrderByChild ? other.order_by_child : empty;
    return std::tie(order_by, lhs_child, start_at_value, start_at_child_key,
                    end_at_value, end_at_child_key, equal_to_value,
                    equal_to_child_key, limit_first, limit_last) <
           std::tie(other.order_by, rhs_child, other.start_at_value,
                    other.start_at_child_key, other.end_at_value,
                    other.end_at_child_key, other.equal_to_value,
                    other.equal_to_child_key, other.limit_first,
                    other.limit_last);
  }

  OrderBy order_by = kOrderByPriority;
  std::string order_by_child;
  std::optional<Variant> start_at_value;
  std::optional<std::string> start_at_child_key;
  std::optional<Variant> end_at_value;
  std::optional<std::string> end_at_child_key;
  std::optional<Variant> equal_to_value;
  std::optional<std::string> equal_to_child_key;
  size_t limit_first = 0;
  size_t limit_last = 0;
};

struct QuerySpec {
  QuerySpec() = default;
  explicit QuerySpec(const Path& _path) : path(_path) {}
  explicit QuerySpec(const QueryParams& _params) : params(_params) {}
  QuerySpec(const Path& _path, const QueryParams& _params)
      : path(_path), params(_params) {}

  bool operator==(const QuerySpec& other) const {
    return path == other.path && params == other.params;
  }
  bool operator<(const QuerySpec& other) const {
    return std::tie(path, params) < std::tie(other.path, other.params);
  }

  Path path;
  QueryParams params;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_COMMON_QUERY_SPEC_H_
