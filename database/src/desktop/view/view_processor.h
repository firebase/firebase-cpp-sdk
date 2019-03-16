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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_PROCESSOR_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_PROCESSOR_H_

#include "app/memory/unique_ptr.h"
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

// A ViewProcessor does the heavy lifting of updating the data on a view when
// changes need to be made. It takes an operation and applies it to the proper
// caches (taking into account things like the source of the operation, client
// or server).
class ViewProcessor {
 public:
  explicit ViewProcessor(UniquePtr<VariantFilter> filter);

  ~ViewProcessor();

  // Apply an operation. This takes an operation, and various bits of data from
  // the View this ViewProcessor is associated with, including the old cache
  // values. It then generates the new values, along with a list of all the
  // changes that were made. The Changes can be used later to generate the
  // Events that need to be fired at the various listeners on the View.
  void ApplyOperation(const ViewCache& old_view_cache,
                      const Operation& operation,
                      const WriteTreeRef& writes_cache,
                      const Variant* opt_complete_cache,
                      ViewCache* out_view_cache,
                      std::vector<Change>* out_changes);

  // Reverts a write operation using data in the cache.
  ViewCache RevertUserWrite(const ViewCache& view_cache, const Path& path,
                            const WriteTreeRef& writes_cache,
                            const Variant* opt_complete_server_cache,
                            ChildChangeAccumulator* accumulator);

 private:
  ViewProcessor() = delete;
  ViewProcessor(const ViewProcessor&) = delete;
  ViewProcessor& operator=(const ViewProcessor&) = delete;

  // Add a ValueChange Event if appropriate.
  void MaybeAddValueEvent(const ViewCache& old_view_cache,
                          const ViewCache& new_view_cache,
                          std::vector<Change>* accumulator);

  // Produce a new ViewCache based on the given old view_cache and
  // writes_cache, and use the accumulator to gather the resulting Changes for
  // later processing.
  ViewCache GenerateEventCacheAfterServerEvent(
      const ViewCache& view_cache, const Path& change_path,
      const WriteTreeRef& writes_cache, const CompleteChildSource* source,
      ChildChangeAccumulator* accumulator);

  // Apply a server overwrite to a location in the database, and return the
  // updated cache.
  ViewCache ApplyServerOverwrite(const ViewCache& old_view_cache,
                                 const Path& change_path,
                                 const Variant& changed_snap,
                                 const WriteTreeRef& writes_cache,
                                 const Variant* opt_complete_cache,
                                 bool filter_server_node,
                                 ChildChangeAccumulator* accumulator);

  // Apply a local overwrite to a location in the database, and return the
  // updated cache.
  ViewCache ApplyUserOverwrite(const ViewCache& old_view_cache,
                               const Path& change_path,
                               const Variant& changed_snap,
                               const WriteTreeRef& writes_cache,
                               const Variant* opt_complete_cache,
                               ChildChangeAccumulator* accumulator);

  // Apply a local merge to a location in the database, and return the
  // updated cache.
  ViewCache ApplyServerMerge(const ViewCache& view_cache, const Path& path,
                             const CompoundWrite& changed_children,
                             const WriteTreeRef& writes_cache,
                             const Variant& server_cache,
                             bool filter_server_node,
                             ChildChangeAccumulator* accumulator);

  // Apply a local merge to a location in the database, and return the
  // updated cache.
  ViewCache ApplyUserMerge(const ViewCache& view_cache, const Path& path,
                           const CompoundWrite& changed_children,
                           const WriteTreeRef& writes_cache,
                           const Variant& server_cache,
                           ChildChangeAccumulator* accumulator);

  // Acknowledge a write made by the user was accepted by the server, and return
  // the updated cache.
  ViewCache AckUserWrite(const ViewCache& view_cache, const Path& ack_path,
                         const Tree<bool>& affected_tree,
                         const WriteTreeRef& writes_cache,
                         const Variant* opt_complete_cache,
                         ChildChangeAccumulator* accumulator);

  // Listening is complete on this location. Update the server cache to refect
  // this.
  ViewCache ListenComplete(const ViewCache& view_cache, const Path& path,
                           const WriteTreeRef& writes_cache,
                           ChildChangeAccumulator* accumulator);

  // A filter on this location. This is generated based on the parameters of the
  // QuerySpect, and is used to determine what fields are visible to the
  // ViewProcessor, pruning the ones that are not.
  UniquePtr<VariantFilter> filter_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_PROCESSOR_H_
