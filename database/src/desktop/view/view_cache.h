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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_CACHE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_CACHE_H_

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

// A CacheNode only stores complete children. Additionally it holds a flag
// whether the node can be considered fully initialized in the sense that we
// know at one point in time this represented a valid state of the world, e.g.
// initialized with data from the server, or a complete overwrite by the client.
// The filtered flag also tracks whether a node potentially had children removed
// due to a filter.
class CacheNode {
 public:
  CacheNode()
      : indexed_variant_(), fully_initialized_(false), filtered_(false) {}

  CacheNode(const IndexedVariant& indexed_variant, bool fully_initialized,
            bool filtered)
      : indexed_variant_(indexed_variant),
        fully_initialized_(fully_initialized),
        filtered_(filtered) {}

  const IndexedVariant& indexed_variant() const { return indexed_variant_; }
  IndexedVariant& indexed_variant() { return indexed_variant_; }

  const Variant& variant() const { return indexed_variant_.variant(); }

  // Returns true if this cache is fully initialized, meaning all values have
  // been pulled down from the server, rather than relying on local cache
  // values.
  bool fully_initialized() const { return fully_initialized_; }

  // Returns true if this cache is filtered in some way by the query parameters
  // that initiated this cache.
  bool filtered() const { return filtered_; }

  // Returns true if the cache is complete (fully initialized and unfiltered) at
  // the given path.
  bool IsCompleteForPath(const Path& path) const {
    if (path.empty()) {
      return fully_initialized_ && !filtered_;
    } else {
      return IsCompleteForChild(path.GetDirectories().front());
    }
  }

  // Returns true if the cache's value at the given key is complete (fully
  // initialized and unfiltered) at.
  bool IsCompleteForChild(const std::string& key) const {
    return (fully_initialized_ && !filtered_) ||
           (GetInternalVariant(&variant(), key) != nullptr);
  }

  // Return the complete variant if this cache is fully initialzed, and null
  // otherwise.
  const Variant* GetCompleteSnap() const {
    return fully_initialized_ ? &variant() : nullptr;
  }

 private:
  IndexedVariant indexed_variant_;

  // Whether the node can be considered fully initialized in the sense that we
  // know at one point in time this represented a valid state of the world
  bool fully_initialized_;

  // Tracks whether a node potentially had children removed due to a filter.
  bool filtered_;
};

inline bool operator==(const CacheNode& lhs, const CacheNode& rhs) {
  return lhs.indexed_variant() == rhs.indexed_variant() &&
         lhs.fully_initialized() == rhs.fully_initialized() &&
         lhs.filtered() == rhs.filtered();
}

inline bool operator!=(const CacheNode& lhs, const CacheNode& rhs) {
  return !(lhs == rhs);
}

// A cache of the data at a location in the database. This contains both a
// snapshot of the last known server value, as well as any local changes that
// have been made that the server may or may not yet be aware of.
class ViewCache {
 public:
  ViewCache() : local_snap_(), server_snap_() {}

  ViewCache(const CacheNode& local_snap, const CacheNode& server_snap)
      : local_snap_(local_snap), server_snap_(server_snap) {}

  // Get the complete snapshot of the local cache, or null if it is not
  // present.
  const Variant* GetCompleteLocalSnap() const {
    return local_snap_.GetCompleteSnap();
  }

  // Get the complete snapshot of the server cache, or null if it is not
  // present.
  const Variant* GetCompleteServerSnap() const {
    return server_snap_.GetCompleteSnap();
  }

  // Create an new ViewCache by populating the local cache with the given data
  // and the server cache with the data from this ViewCache. This ViewCache
  // remains unchanged.
  ViewCache UpdateLocalSnap(const IndexedVariant& local_snap, bool complete,
                            bool filtered) const {
    return ViewCache(CacheNode(local_snap, complete, filtered), server_snap_);
  }

  // Create an new ViewCache by populating the local cache with the data from
  // this ViewCache and the server cache with the given data. This ViewCache
  // remains unchanged.
  ViewCache UpdateServerSnap(const IndexedVariant& server_snap, bool complete,
                             bool filtered) const {
    return ViewCache(local_snap_, CacheNode(server_snap, complete, filtered));
  }

  const CacheNode& local_snap() const { return local_snap_; }
  CacheNode& local_snap() { return local_snap_; }

  const CacheNode& server_snap() const { return server_snap_; }
  CacheNode& server_snap() { return server_snap_; }

 private:
  // Snapshot of what the local view is of this location.
  CacheNode local_snap_;

  // Snapshot of what the expected server values are, in case we need to revert.
  CacheNode server_snap_;
};

inline bool operator==(const ViewCache& lhs, const ViewCache& rhs) {
  return lhs.local_snap() == rhs.local_snap() &&
         lhs.server_snap() == rhs.server_snap();
}

inline bool operator!=(const ViewCache& lhs, const ViewCache& rhs) {
  return !(lhs == rhs);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_CACHE_H_
