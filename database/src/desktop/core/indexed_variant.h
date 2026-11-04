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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CORE_INDEXED_VARIANT_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CORE_INDEXED_VARIANT_H_

#include <set>
#include <string>
#include <utility>

#include "app/src/include/firebase/variant.h"
#include <optional>
#include "database/src/common/query_spec.h"
#include "database/src/desktop/query_params_comparator.h"

namespace firebase::database::internal {

class IndexedVariantGetOrderByVariantTest;

// Represents a Variant together with an index. The index and variant are
// updated in unison. The index represents the order elements of a variant map
// should be in according to the QueryParams's ordering.
class IndexedVariant {
 public:
  using Index =
      std::set<std::pair<const Variant, const Variant>, QueryParamsLesser>;

  IndexedVariant();
  explicit IndexedVariant(const Variant& variant);
  IndexedVariant(const Variant& variant, const QueryParams& query_params);
  IndexedVariant(Variant&& variant, const QueryParams& query_params);

  IndexedVariant(const IndexedVariant& other);
  IndexedVariant& operator=(const IndexedVariant& other);

  IndexedVariant(IndexedVariant&& other) noexcept;
  IndexedVariant& operator=(IndexedVariant&& other) noexcept;

  const QueryParams& query_params() const { return query_params_; }
  const Variant& variant() const { return variant_; }

  const Index& index() const { return index_; }

  // Find an element in the index.
  Index::const_iterator Find(const Variant& key) const;

  // Return the name of the child immediately prior to the given child. This
  // requires both the name and the value as the sorting of an IndexedVariant
  // can sort by either. Returns nullptr if there is no previous child.
  // This function expecting the following cases:
  // 1) the given child key is not found in index_ => return nullptr
  // 2) the given child key and child value pair is found and the same to the
  //    one in index_ => return the previous child name if any
  // 3) any other cases are unexpected and the behavior is undefined.  Ex.
  //    if the given child key is found but the given child_value has different
  //    priority to the one in index_.  This should be prevented by the caller.
  const char* GetPredecessorChildName(const std::string& child_key,
                                      const Variant& child_value) const;

  // Set the value of the child give by 'key' to 'child'.
  // If this variant is not a map, it will be converted into one in the process.
  IndexedVariant UpdateChild(const std::string& key,
                             const Variant& child) const;

  // Updates the priority of this indexed variant to the given value.
  IndexedVariant UpdatePriority(const Variant& priority) const;

  // Gets the first child in the indexed variant, if one is present. If this is
  // a leaf node, an empty std::optional is returned
  std::optional<std::pair<Variant, Variant>> GetFirstChild() const;

  // Gets the last child in the indexed variant, if one is present. If this is
  // a leaf node, an empty std::optional is returned
  std::optional<std::pair<Variant, Variant>> GetLastChild() const;

 private:
  void EnsureIndexed();

  // Return the variant to use when using OrderBy on this element.
  // This function does NOT prune the priority from the result if it is a map
  // because the return value is only used to compare with a fundamental type,
  // ex. QueryParams::equal_to_value.
  const Variant* GetOrderByVariant(const Variant& key,
                                   const Variant& value) const;

  // Return if the key-value pair is within range given QueryParams based on
  // order_by, order_by_child, equal_to_(value|child_key),
  // start_at_(value|child_key) and end_at_(value|child_key).
  // This does not take limit_first and limit_last into account.
  bool IsKeyValueInRange(const QueryParams& qs, const Variant& key,
                         const Variant& value) const;

  // The raw variant underlying this IndexedVariant. When a Variant represents a
  // map, it doesn't organize the map's elements according to the QueryParams.
  // That's why we keep a separate Index that is ordered by the parameters in
  // the QueryParams for when the elements need to be iterated over in order.
  Variant variant_;

  // The query params that contains the ordering rules.
  QueryParams query_params_;

  // An ordered set of key/value Variant pairs stored by value and ordered by
  // QueryParamsLesser(&query_params_). To keep index_ and variant_ in sync,
  // all updates should be funneled through UpdateChild or UpdatePriority.
  Index index_;

  friend class IndexedVariantGetOrderByVariantTest;
};

bool operator==(const IndexedVariant& lhs, const IndexedVariant& rhs);
bool operator!=(const IndexedVariant& lhs, const IndexedVariant& rhs);

}  // namespace firebase::database::internal

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CORE_INDEXED_VARIANT_H_
