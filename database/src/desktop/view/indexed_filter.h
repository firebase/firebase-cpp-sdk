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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_INDEXED_FILTER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_INDEXED_FILTER_H_

#include <string>
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/view/child_change_accumulator.h"
#include "database/src/desktop/view/variant_filter.h"

namespace firebase {
namespace database {
namespace internal {

// VariantFilters are generally responsible for filtering elements based on the
// options specified in the QueryParams. However, the IndexedFilter is different
// in that it does not filter variants but rather it just applies an index to
// the variant and keeps track of any changes.
class IndexedFilter : public VariantFilter {
 public:
  explicit IndexedFilter(const QueryParams& query_params)
      : VariantFilter(query_params) {}

  ~IndexedFilter() override;

  IndexedVariant UpdateChild(
      const IndexedVariant& indexed_variant, const std::string& key,
      const Variant& new_child, const Path& affected_path,
      const CompleteChildSource* source,
      ChildChangeAccumulator* opt_change_accumulator) const override;

  IndexedVariant UpdateFullVariant(
      const IndexedVariant& old_snap, const IndexedVariant& new_snap,
      ChildChangeAccumulator* opt_change_accumulator) const override;

  IndexedVariant UpdatePriority(const IndexedVariant& old_snap,
                                const Variant& new_priority) const override;

  const VariantFilter* GetIndexedFilter() const override;

  bool FiltersVariants() const override;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_INDEXED_FILTER_H_
