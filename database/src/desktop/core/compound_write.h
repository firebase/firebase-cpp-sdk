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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CORE_COMPOUND_WRITE_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CORE_COMPOUND_WRITE_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/desktop/core/tree.h"

namespace firebase {
namespace database {
namespace internal {

class CompoundWrite {
 public:
  CompoundWrite() = default;
  explicit CompoundWrite(const Tree<Variant>& write_tree)
      : write_tree_(write_tree) {}

  static CompoundWrite FromChildMerge(
      const std::map<std::string, Variant>& merge);
  static CompoundWrite FromVariantMerge(const Variant& merge);
  static CompoundWrite FromPathMerge(const std::map<Path, Variant>& merge);
  static CompoundWrite EmptyWrite() { return CompoundWrite(); }

  CompoundWrite AddWrite(const Path& path,
                         const std::optional<Variant>& value) const;
  CompoundWrite AddWrite(const std::string& key,
                         const std::optional<Variant>& value) const {
    return AddWrite(Path(key), value);
  }

  void AddWriteInline(const Path& path, const std::optional<Variant>& variant);
  void AddWriteInline(const std::string& key,
                      const std::optional<Variant>& value) {
    AddWriteInline(Path(key), value);
  }

  CompoundWrite AddWrites(const Path& path, const CompoundWrite& updates) const;
  void AddWritesInline(const Path& path, const CompoundWrite& updates);

  CompoundWrite RemoveWrite(const Path& path) const;
  void RemoveWriteInline(const Path& path);

  bool HasCompleteWrite(const Path& path) const {
    return GetCompleteVariant(path).has_value();
  }
  const std::optional<Variant>& GetRootWrite() const {
    return write_tree_.value();
  }
  std::optional<Variant> GetCompleteVariant(const Path& path) const;
  std::vector<std::pair<Variant, Variant>> GetCompleteChildren() const;

  CompoundWrite ChildCompoundWrite(const Path& path) const;
  std::map<std::string, CompoundWrite> ChildCompoundWrites() const;
  bool IsEmpty() const { return write_tree_.IsEmpty(); }
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

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CORE_COMPOUND_WRITE_H_
