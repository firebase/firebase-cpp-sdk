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

#include "database/src/desktop/core/compound_write.h"

#include "app/src/assert.h"
#include "database/src/desktop/core/tree.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

CompoundWrite CompoundWrite::FromChildMerge(
    const std::map<std::string, Variant>& merge) {
  Tree<Variant> write_tree;
  for (const auto& [key, val] : merge) write_tree.SetValueAt(Path(key), val);
  return CompoundWrite(write_tree);
}

CompoundWrite CompoundWrite::FromVariantMerge(const Variant& merge) {
  Tree<Variant> write_tree;
  if (merge.is_map()) {
    for (const auto& [key, val] : merge.map()) {
      write_tree.SetValueAt(Path(key.AsString().string_value()), val);
    }
  } else {
    write_tree.set_value(merge);
  }
  return CompoundWrite(write_tree);
}

CompoundWrite CompoundWrite::FromPathMerge(
    const std::map<Path, Variant>& merge) {
  Tree<Variant> write_tree;
  for (const auto& [path, val] : merge) write_tree.SetValueAt(path, val);
  return CompoundWrite(write_tree);
}

CompoundWrite CompoundWrite::AddWrite(
    const Path& path, const std::optional<Variant>& variant) const {
  CompoundWrite target = *this;
  target.AddWriteInline(path, variant);
  return target;
}

void CompoundWrite::AddWriteInline(const Path& path,
                                   const std::optional<Variant>& variant) {
  if (path.empty()) {
    *this = CompoundWrite(Tree<Variant>(variant));
    return;
  }
  std::optional<Path> root_most_path =
      write_tree_.FindRootMostPathWithValue(path);
  if (!root_most_path.has_value()) {
    write_tree_.SetValueAt(path, variant);
    return;
  }
  auto relative_path = Path::GetRelative(*root_most_path, path);
  const Variant* value = write_tree_.GetValueAt(*root_most_path);
  std::vector<std::string> directories = relative_path->GetDirectories();
  std::string back = directories.empty() ? "" : directories.back();
  if (!relative_path->empty() && IsPriorityKey(back) &&
      VariantIsEmpty(VariantGetChild(value, relative_path->GetParent()))) {
    return;
  }
  Variant updated_variant = *value;
  VariantUpdateChild(&updated_variant, *relative_path, *variant);
  write_tree_.SetValueAt(*root_most_path, updated_variant);
}

CompoundWrite CompoundWrite::AddWrites(const Path& path,
                                       const CompoundWrite& updates) const {
  CompoundWrite target = *this;
  target.AddWritesInline(path, updates);
  return target;
}

void CompoundWrite::AddWritesInline(const Path& path,
                                    const CompoundWrite& updates) {
  updates.write_tree_.Fold(
      0, [&path, this](Path relative_path, Variant value, int) {
        AddWriteInline(path.GetChild(relative_path),
                       std::optional<Variant>(value));
        return 0;
      });
}

CompoundWrite CompoundWrite::RemoveWrite(const Path& path) const {
  if (path.empty()) return CompoundWrite();
  CompoundWrite result(*this);
  result.RemoveWriteInline(path);
  return result;
}

void CompoundWrite::RemoveWriteInline(const Path& path) {
  if (path.empty()) {
    *this = CompoundWrite();
  } else if (Tree<Variant>* subtree = write_tree_.GetChild(path)) {
    subtree->children().clear();
    subtree->value().reset();
  }
}

std::optional<Variant> CompoundWrite::GetCompleteVariant(
    const Path& path) const {
  std::optional<Path> root_most = write_tree_.FindRootMostPathWithValue(path);
  if (!root_most.has_value()) return std::nullopt;
  const Variant* root_most_value = write_tree_.GetValueAt(*root_most);
  auto remaining_path = Path::GetRelative(*root_most, path);
  return VariantGetChild(root_most_value, *remaining_path);
}

std::vector<std::pair<Variant, Variant>> CompoundWrite::GetCompleteChildren()
    const {
  std::vector<std::pair<Variant, Variant>> children;
  if (GetRootWrite().has_value()) {
    const Variant* value = GetVariantValue(&write_tree_.value().value());
    if (value->is_map()) {
      for (const auto& entry : value->map()) children.push_back(entry);
    }
  } else {
    for (const auto& [key, subtree] : write_tree_.children()) {
      if (subtree.value().has_value()) {
        children.emplace_back(key, *subtree.value());
      }
    }
  }
  return children;
}

CompoundWrite CompoundWrite::ChildCompoundWrite(const Path& path) const {
  if (path.empty()) return *this;
  std::optional<Variant> shadowing_variant = GetCompleteVariant(path);
  if (shadowing_variant.has_value()) {
    return CompoundWrite(Tree<Variant>(shadowing_variant));
  }
  const Tree<Variant>* subtree = write_tree_.GetChild(path);
  return subtree ? CompoundWrite(*subtree) : CompoundWrite();
}

std::map<std::string, CompoundWrite> CompoundWrite::ChildCompoundWrites()
    const {
  std::map<std::string, CompoundWrite> children;
  for (const auto& [key, subtree] : write_tree_.children()) {
    children[key] = CompoundWrite(subtree);
  }
  return children;
}

Variant CompoundWrite::Apply(const Variant& variant) const {
  return ApplySubtreeWrite(Path::GetRoot(), &write_tree_, variant);
}

Variant CompoundWrite::ApplySubtreeWrite(const Path& relative_path,
                                         const Tree<Variant>* write_tree,
                                         Variant variant) const {
  if (write_tree->value().has_value()) {
    VariantUpdateChild(&variant, relative_path, *write_tree->value());
    return variant;
  }
  std::optional<Variant> priority_write;
  for (const auto& [child_key, child_tree] : write_tree->children()) {
    if (IsPriorityKey(child_key)) {
      FIREBASE_DEV_ASSERT_MESSAGE(
          child_tree.children().empty(),
          "Priority writes must always be leaf variants");
      priority_write = child_tree.value();
    } else {
      variant = ApplySubtreeWrite(relative_path.GetChild(child_key),
                                  &child_tree, variant);
    }
  }
  if (!VariantIsEmpty(VariantGetChild(&variant, relative_path)) &&
      priority_write.has_value()) {
    VariantUpdateChild(&variant, relative_path.GetChild(kPriorityKey),
                       *priority_write);
  }
  return variant;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
