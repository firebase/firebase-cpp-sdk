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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VARIANT_FILTER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VARIANT_FILTER_H_

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/child_change_accumulator.h"

namespace firebase {
namespace database {
namespace internal {

// Since updates to filtered variants might require variants to be pulled in
// from "outside" the variant, this interface can help to get complete
// children that can be pulled in. A class implementing this interface takes
// potentially multiple sources (e.g. user writes, server data from other
// views etc.) to try its best to get a complete child that might be useful
// in pulling into the view.
class CompleteChildSource {
 public:
  virtual ~CompleteChildSource() {}
  virtual Optional<Variant> GetCompleteChild(
      const std::string& child_key) const = 0;

  virtual Optional<std::pair<Variant, Variant>> GetChildAfterChild(
      const QueryParams& query_params, const std::pair<Variant, Variant>& child,
      IterationDirection direction) const = 0;
};

// VariantFilter is used to update variants and complete children of variants
// while applying queries on the fly and keeping track of any child changes.
// This class does not track value changes as value changes depend on more than
// just the variant itself. Different kinds of queries require different kinds
// of implementations of this interface.
class VariantFilter {
 public:
  explicit VariantFilter(const QueryParams& query_params)
      : query_params_(query_params) {}

  virtual ~VariantFilter() {}

  // Update a single complete child in the snap. If the child equals the old
  // child in the snap, this is a no-op. The method expects an indexed snap.
  virtual IndexedVariant UpdateChild(
      const IndexedVariant& indexed_variant, const std::string& key,
      const Variant& new_child, const Path& affected_path,
      const CompleteChildSource* source,
      ChildChangeAccumulator* opt_change_accumulator) const = 0;

  // Update a variant in full and output any resulting change from this
  // complete update.
  virtual IndexedVariant UpdateFullVariant(
      const IndexedVariant& old_snap, const IndexedVariant& new_snap,
      ChildChangeAccumulator* opt_change_accumulator) const = 0;

  // Update the priority of the root variant
  virtual IndexedVariant UpdatePriority(const IndexedVariant& old_snap,
                                        const Variant& new_priority) const = 0;

  // Returns true if children might be filtered due to query criteria.
  virtual bool FiltersVariants() const = 0;

  // Returns the index filter that this filter uses to get a VariantFilter that
  // doesn't filter any children.
  const virtual VariantFilter* GetIndexedFilter() const = 0;

  const QueryParams& query_params() const { return query_params_; }

 private:
  const QueryParams query_params_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VARIANT_FILTER_H_
