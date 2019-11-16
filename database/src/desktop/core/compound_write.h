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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_COMPOUND_WRITE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_COMPOUND_WRITE_H_

#include <map>
#include <string>
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/desktop/core/tree.h"

namespace firebase {
namespace database {
namespace internal {

// This class holds a collection of writes that can be applied to nodes in
// unison. It abstracts away the logic with dealing with priority writes and
// multiple nested writes. At any given path there is only allowed to be one
// write modifying that path. Any write to an existing path or shadowing an
// existing path will modify that existing write to reflect the write added.
class CompoundWrite {
 public:
  CompoundWrite() : write_tree_() {}

  // Create a compound write from a tree of variants, where each variant in the
  // tree represents a write at that location.
  explicit CompoundWrite(const Tree<Variant>& write_tree)
      : write_tree_(write_tree) {}

  // Create a CompoundWrite from a map of strings (that represent database
  // Paths) to Variants, where each variant in the map represents a write at the
  // associated location.
  static CompoundWrite FromChildMerge(
      const std::map<std::string, Variant>& merge);

  // Create a CompoundWrite from a map of Variants to Variants, where each
  // Variant value in the map represents a write at the associated location. The
  // location is given by a string Variant representing a database path.
  static CompoundWrite FromVariantMerge(const Variant& merge);

  // Create a CompoundWrite from a map of Paths to Variants, where each Variant
  // in the map represents a write at the associated Path location.
  static CompoundWrite FromPathMerge(const std::map<Path, Variant>& merge);

  // Create an empty CompoundWrite.
  static CompoundWrite EmptyWrite();

  // Create a new CompoundWrite that incorperates the new value to write at the
  // given path.
  CompoundWrite AddWrite(const Path& path,
                         const Optional<Variant>& value) const;
  CompoundWrite AddWrite(const Path& path, const Variant& value) const;
  CompoundWrite AddWrite(const std::string& key,
                         const Optional<Variant>& value) const;
  CompoundWrite AddWrite(const std::string& key, const Variant& value) const;

  // Incorperate the new value to write at the given path.
  void AddWriteInline(const Path& path, const Optional<Variant>& variant);
  void AddWriteInline(const Path& path, const Variant& value);
  void AddWriteInline(const std::string& key,
    const Optional<Variant>& value);
  void AddWriteInline(const std::string& key, const Variant& value);

  // Create a new CompoundWrite that incorperates all of the writes in the given
  // CompoundWrite at the given path.
  CompoundWrite AddWrites(const Path& path, const CompoundWrite& updates) const;

  // Incorperate all of the writes in the given CompoundWrite at the given path.
  void AddWritesInline(const Path& path, const CompoundWrite& updates);

  // Will remove a write at the given path and deeper paths. This will not
  // modify a write at a higher location, which must be removed by calling this
  // method with that path.
  //
  // Returns the new WriteCompound with the removed path
  CompoundWrite RemoveWrite(const Path& path) const;

  // Will remove a write at the given path and deeper paths. This will not
  // modify a write at a higher location, which must be removed by calling this
  // method with that path.
  void RemoveWriteInline(const Path& path);

  // Returns whether this CompoundWrite will fully overwrite a node at a given
  // location and can therefore be considered "complete".
  bool HasCompleteWrite(const Path& path) const;

  const Optional<Variant>& GetRootWrite() const;

  // Returns a node for a path if and only if the node is a "complete" overwrite
  // at that path. This will not aggregate writes from deeper paths, but will
  // return child nodes from a more shallow path.
  Optional<Variant> GetCompleteVariant(const Path& path) const;

  // Returns all children that are guaranteed to be a complete overwrite.
  std::vector<std::pair<Variant, Variant>> GetCompleteChildren() const;

  // Return a CompoundWrite consisting of the changes at or below the given
  // path.
  CompoundWrite ChildCompoundWrite(const Path& path) const;

  // Return a map of each immediate child in the tree and the write that is
  // going to occur on that location.
  std::map<std::string, CompoundWrite> ChildCompoundWrites() const;

  // Returns true if this CompoundWrite is empty and therefore does not modify
  // any nodes.
  bool IsEmpty() const;

  // Applies this CompoundWrite to a Variant. The node is returned with all
  // writes from this CompoundWrite applied to the variant.
  Variant Apply(const Variant& variant) const;

  const Tree<Variant>& write_tree() const { return write_tree_; }

  bool operator==(const CompoundWrite& other) const {
    return write_tree_ == other.write_tree_;
  }

  bool operator!=(const CompoundWrite& other) const {
    return !(*this == other);
  }

 private:
  Variant ApplySubtreeWrite(const Path& relative_path,
                            const Tree<Variant>* write_tree,
                            Variant node) const;

  Tree<Variant> write_tree_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_COMPOUND_WRITE_H_
