// Copyright 2018 Google LLC
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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_QUERY_PARAMS_COMPARATOR_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_QUERY_PARAMS_COMPARATOR_H_

#include <map>
#include "app/src/include/firebase/variant.h"
#include "database/src/common/query_spec.h"

namespace firebase {
namespace database {
namespace internal {

// A Variant comparator, only meant for internal use.
//
// Explanation: Variants by default sort their elements into a map using a
// custom sorting. In order to use mimic the ordering rules used by the RTDB,
// we need to be able to organize them accoring to the given QueryParams, which
// specifies how a query should be ordered.
//
//  * If the QueryParams specifies order_by == kOrderByPriority:
//      * The priority variants of the two values are compared according to the
//        kOrderByValue rules. If the priority variants are equal, the keys are
//        compared using the kOrderByKey rules.
//  * If the QueryParams specifies order_by == kOrderByChild:
//      * The values of the children named in QueryParams::path are compared
//        according to the kOrderByValue rules. If the priority variants are
//        equal, the keys are compared using the kOrderByKey rules.
//  * If the QueryParams specifies order_by == kOrderByKey:
//      * If the keys are identical, return 0.
//      * If the first key is the special kMinKey value, return -1.
//      * If the second key is the special kMaxKey value, return -1.
//      * If the second key is the special kMinKey value, return 1.
//      * If the first key is the special kMaxKey value, return 1.
//      * If the first key is an integer and the second key isn't, return -1.
//      * If the second key is an integer and the first key isn't, return 1.
//      * If both keys are integers, return -1 if the first key is less and 1 if
//        the second key is less.
//      * Compare the two strings using strcmp.
//  * If the QueryParams specifies order_by == kOrderByValue:
//      * If different types, compare type as int: v2.type() - v1.type()
//      * If both are null, return 0.
//      * If both are boolean, false is considered smaller than true.
//      * If both are numbers (integers or floating points) they are compared
//        (casting to doubles if necessary)
//      * If both are strings, the values are compared using strcmp.
//      * If both are maps, return 0.
//    If the result ended up being 0, the keys are compared using the
//    kOrderByKey rules.
class QueryParamsComparator {
 public:
  QueryParamsComparator() : query_params_(nullptr) {}

  explicit QueryParamsComparator(const QueryParams* query_params)
      : query_params_(query_params) {}

  // Compare two database values given their key and value.
  int Compare(const Variant& key_a, const Variant& value_a,
              const Variant& key_b, const Variant& value_b) const;

  // Compare two database values given their key and value.
  int Compare(const std::pair<Variant, Variant>& a,
              const std::pair<Variant, Variant>& b) const {
    return Compare(a.first, a.second, b.first, b.second);
  }

  // Utility function to compare two variants as keys.
  static int CompareKeys(const Variant& key_a, const Variant& key_b);
  // Utility function to compare two variants as values.
  static int CompareValues(const Variant& variant_a, const Variant& variant_b);
  // Utility function to compare two variant priorities.
  static int ComparePriorities(const Variant& value_a, const Variant& value_b);

  // Special values for the minimum and maximum keys and values a node can have.
  // These values will always be sorted before or after all over values.
  static const char kMinKey[];
  static const char kMaxKey[];
  static const std::pair<Variant, Variant> kMinNode;
  static const std::pair<Variant, Variant> kMaxNode;

 private:
  int CompareChildren(const Variant& value_a, const Variant& value_b) const;

  const QueryParams* query_params_;
};

// A helper class that allows you to use a QueryParamsComparator in a std::set
// as the std::set's comparator argument.
class QueryParamsLesser {
 public:
  QueryParamsLesser() : comparator_() {}
  explicit QueryParamsLesser(const QueryParams* query_params)
      : comparator_(query_params) {}

  bool operator()(const std::pair<Variant, Variant>& a,
                  const std::pair<Variant, Variant>& b) const {
    return comparator_.Compare(a.first, a.second, b.first, b.second) < 0;
  }

  bool operator()(const std::pair<const Variant, const Variant>& a,
                  const std::pair<const Variant, const Variant>& b) const {
    return comparator_.Compare(a.first, a.second, b.first, b.second) < 0;
  }

  bool operator()(const std::pair<Variant*, Variant*>& a,
                  const std::pair<Variant*, Variant*>& b) const {
    return comparator_.Compare(*a.first, *a.second, *b.first, *b.second) < 0;
  }

  bool operator()(const std::pair<const Variant*, const Variant*>& a,
                  const std::pair<const Variant*, const Variant*>& b) const {
    return comparator_.Compare(*a.first, *a.second, *b.first, *b.second) < 0;
  }

 private:
  QueryParamsComparator comparator_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_QUERY_PARAMS_COMPARATOR_H_
