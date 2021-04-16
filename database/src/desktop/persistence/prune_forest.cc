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

#include "database/src/desktop/persistence/prune_forest.h"

#include <set>
#include <string>

#include "app/src/assert.h"
#include "app/src/path.h"
#include "database/src/desktop/core/tree.h"

namespace firebase {
namespace database {
namespace internal {

static const Tree<bool> kPruneTree(true);  // NOLINT
static const Tree<bool> kKeepTree(false);  // NOLINT

static bool KeepPredicate(bool prune) { return !prune; }
static bool PrunePredicate(bool prune) { return prune; }

PruneForestRef::PruneForestRef(PruneForest* prune_forest)
    : prune_forest_(prune_forest) {}

bool PruneForestRef::operator==(const PruneForestRef& other) const {
  return (prune_forest_ == other.prune_forest_) ||
         (prune_forest_ != nullptr && other.prune_forest_ != nullptr &&
          *prune_forest_ == *other.prune_forest_);
}

bool PruneForestRef::PrunesAnything() const {
  return prune_forest_->ContainsMatchingValue(PrunePredicate);
}

bool PruneForestRef::ShouldPruneUnkeptDescendants(const Path& path) const {
  const bool* should_prune = prune_forest_->LeafMostValue(path);
  return should_prune != nullptr && *should_prune;
}

bool PruneForestRef::ShouldKeep(const Path& path) const {
  const bool* should_prune = prune_forest_->LeafMostValue(path);
  return should_prune != nullptr && !*should_prune;
}

bool PruneForestRef::AffectsPath(const Path& path) const {
  return prune_forest_->RootMostValue(path) != nullptr ||
         (prune_forest_->GetChild(path) &&
          !prune_forest_->GetChild(path)->IsEmpty());
}

PruneForestRef PruneForestRef::GetChild(const std::string& key) {
  return PruneForestRef(prune_forest_->GetOrMakeSubtree(Path(key)));
}

const PruneForestRef PruneForestRef::GetChild(const std::string& key) const {
  return PruneForestRef(prune_forest_->GetOrMakeSubtree(Path(key)));
}

PruneForestRef PruneForestRef::GetChild(const Path& path) {
  return PruneForestRef(prune_forest_->GetOrMakeSubtree(path));
}

const PruneForestRef PruneForestRef::GetChild(const Path& path) const {
  return PruneForestRef(prune_forest_->GetOrMakeSubtree(path));
}

void PruneForestRef::Prune(const Path& path) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      prune_forest_->RootMostValueMatching(path, KeepPredicate) == nullptr,
      "Can't prune path that was kept previously!");
  if (prune_forest_->RootMostValueMatching(path, PrunePredicate) != nullptr) {
    // This path will already be pruned
  } else {
    *prune_forest_->GetOrMakeSubtree(path) = kPruneTree;
  }
}

void PruneForestRef::Keep(const Path& path) {
  if (prune_forest_->RootMostValueMatching(path, KeepPredicate) != nullptr) {
    // This path will already be kept
  } else {
    *prune_forest_->GetOrMakeSubtree(path) = kKeepTree;
  }
}

void PruneForestRef::KeepAll(const Path& path,
                             const std::set<std::string>& children) {
  if (prune_forest_->RootMostValueMatching(path, KeepPredicate) != nullptr) {
    // This path will already be kept.
  } else {
    DoAll(path, children, kKeepTree);
  }
}

void PruneForestRef::PruneAll(const Path& path,
                              const std::set<std::string>& children) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      prune_forest_->RootMostValueMatching(path, KeepPredicate) == nullptr,
      "Can't prune path that was kept previously!");

  if (prune_forest_->RootMostValueMatching(path, PrunePredicate) != nullptr) {
    // This path will already be pruned.
  } else {
    DoAll(path, children, kPruneTree);
  }
}

void PruneForestRef::DoAll(const Path& path,
                           const std::set<std::string>& children,
                           const Tree<bool>& keep_or_prune_tree) {
  Tree<bool>* subtree = prune_forest_->GetChild(path);
  for (const std::string& directory : children) {
    *subtree->GetOrMakeSubtree(Path(directory)) = keep_or_prune_tree;
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
