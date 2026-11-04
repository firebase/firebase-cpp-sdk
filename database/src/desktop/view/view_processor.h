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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_VIEW_VIEW_PROCESSOR_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_VIEW_VIEW_PROCESSOR_H_

#include <memory>
#include <vector>

#include "app/src/path.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/core/tree.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/child_change_accumulator.h"
#include "database/src/desktop/view/variant_filter.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

class NoCompleteSource;

class ViewProcessor {
 public:
  explicit ViewProcessor(std::unique_ptr<VariantFilter> filter);
  ~ViewProcessor();

  void ApplyOperation(const ViewCache& old_view_cache,
                      const Operation& operation,
                      const WriteTreeRef& writes_cache,
                      const Variant* opt_complete_cache,
                      ViewCache* out_view_cache,
                      std::vector<Change>* out_changes);

  ViewCache RevertUserWrite(const ViewCache& view_cache, const Path& path,
                            const WriteTreeRef& writes_cache,
                            const Variant* opt_complete_server_cache,
                            ChildChangeAccumulator* accumulator);

 private:
  ViewProcessor() = delete;
  ViewProcessor(const ViewProcessor&) = delete;
  ViewProcessor& operator=(const ViewProcessor&) = delete;

  void MaybeAddValueEvent(const ViewCache& old_view_cache,
                          const ViewCache& new_view_cache,
                          std::vector<Change>* accumulator);
  ViewCache GenerateEventCacheAfterServerEvent(
      const ViewCache& view_cache, const Path& change_path,
      const WriteTreeRef& writes_cache, const CompleteChildSource* source,
      ChildChangeAccumulator* accumulator);
  ViewCache ApplyServerOverwrite(const ViewCache& old_view_cache,
                                 const Path& change_path,
                                 const Variant& changed_snap,
                                 const WriteTreeRef& writes_cache,
                                 const Variant* opt_complete_cache,
                                 bool filter_server_node,
                                 ChildChangeAccumulator* accumulator);
  ViewCache ApplyUserOverwrite(const ViewCache& old_view_cache,
                               const Path& change_path,
                               const Variant& changed_snap,
                               const WriteTreeRef& writes_cache,
                               const Variant* opt_complete_cache,
                               ChildChangeAccumulator* accumulator);
  ViewCache ApplyServerMerge(const ViewCache& view_cache, const Path& path,
                             const CompoundWrite& changed_children,
                             const WriteTreeRef& writes_cache,
                             const Variant* opt_complete_cache,
                             bool filter_server_node,
                             ChildChangeAccumulator* accumulator);
  ViewCache ApplyUserMerge(const ViewCache& view_cache, const Path& path,
                           const CompoundWrite& changed_children,
                           const WriteTreeRef& writes_cache,
                           const Variant* opt_complete_cache,
                           ChildChangeAccumulator* accumulator);
  ViewCache AckUserWrite(const ViewCache& view_cache, const Path& ack_path,
                         const Tree<bool>& affected_tree,
                         const WriteTreeRef& writes_cache,
                         const Variant* opt_complete_cache,
                         ChildChangeAccumulator* accumulator);
  ViewCache ListenComplete(const ViewCache& view_cache, const Path& path,
                           const WriteTreeRef& writes_cache,
                           ChildChangeAccumulator* accumulator);

  std::unique_ptr<VariantFilter> filter_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_VIEW_VIEW_PROCESSOR_H_
