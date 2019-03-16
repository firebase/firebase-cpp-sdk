// Copyright 2019 Google LLC
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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_LIMITED_FILTER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_LIMITED_FILTER_H_

#include <string>
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/view/child_change_accumulator.h"
#include "database/src/desktop/view/ranged_filter.h"
#include "database/src/desktop/view/variant_filter.h"

namespace firebase {
namespace database {
namespace internal {

class LimitedFilter : public VariantFilter {
 public:
  explicit LimitedFilter(const QueryParams& query_params);

  ~LimitedFilter() override;

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

 private:
  IndexedVariant FullLimitUpdateChild(
      const IndexedVariant& old_indexed, const std::string& childKey,
      const Variant& childSnap, const CompleteChildSource* source,
      ChildChangeAccumulator* opt_change_accumulator) const;

  UniquePtr<RangedFilter> ranged_filter_;
  int limit_;
  bool reverse_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_LIMITED_FILTER_H_
