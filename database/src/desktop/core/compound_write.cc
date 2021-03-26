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
  for (const auto& string_variant_pair : merge) {
    write_tree.SetValueAt(Path(string_variant_pair.first),
                          string_variant_pair.second);
  }
  return CompoundWrite(write_tree);
}

CompoundWrite CompoundWrite::FromVariantMerge(const Variant& merge) {
  Tree<Variant> write_tree;
  if (merge.is_map()) {
    for (const auto& variant_variant_pair : merge.map()) {
      write_tree.SetValueAt(
          Path(variant_variant_pair.first.AsString().string_value()),
          variant_variant_pair.second);
    }
  } else {
    write_tree.set_value(merge);
  }
  return CompoundWrite(write_tree);
}

CompoundWrite CompoundWrite::FromPathMerge(
    const std::map<Path, Variant>& merge) {
  Tree<Variant> write_tree;
  for (const auto& string_variant_pair : merge) {
    write_tree.SetValueAt(string_variant_pair.first,
                          string_variant_pair.second);
  }
  return CompoundWrite(write_tree);
}

CompoundWrite CompoundWrite::EmptyWrite() { return CompoundWrite(); }

CompoundWrite CompoundWrite::AddWrite(const Path& path,
                                      const Optional<Variant>& variant) const {
  CompoundWrite target = *this;
  target.AddWriteInline(path, variant);
  return target;
}

void CompoundWrite::AddWriteInline(const Path& path,
                                   const Optional<Variant>& variant) {
  if (path.empty()) {
    *this = CompoundWrite(Tree<Variant>(variant));
  } else {
    Optional<Path> root_most_path = write_tree_.FindRootMostPathWithValue(path);
    if (root_most_path.has_value()) {
      // GetRelative is guaranteed to succeed - the call to
      // FindRootMostPathWithValue is always going to get the beginning segment
      // of the path, so this call just gets the remainder.
      // TODO(amablue): Consider making FindRootMostPathWithValue also return
      // the remainder and not just the root most path.
      Optional<Path> relative_path = Path::GetRelative(*root_most_path, path);
      const Variant* value = write_tree_.GetValueAt(*root_most_path);
      std::vector<std::string> directories = relative_path->GetDirectories();
      std::string back = directories.empty() ? "" : directories.back();

      if (!relative_path->empty() && IsPriorityKey(back) &&
          VariantIsEmpty(VariantGetChild(value, relative_path->GetParent()))) {
        // Ignore priority updates on empty variants
      } else {
        Variant updated_variant = *value;
        VariantUpdateChild(&updated_variant, *relative_path, *variant);
        write_tree_.SetValueAt(*root_most_path, updated_variant);
      }
    } else {
      write_tree_.SetValueAt(path, variant);
    }
  }
}

CompoundWrite CompoundWrite::AddWrite(const Path& path,
                                      const Variant& value) const {
  return AddWrite(path, Optional<Variant>(value));
}

CompoundWrite CompoundWrite::AddWrite(const std::string& key,
                                      const Optional<Variant>& value) const {
  return AddWrite(Path(key), value);
}

CompoundWrite CompoundWrite::AddWrite(const std::string& key,
                                      const Variant& value) const {
  return AddWrite(Path(key), Optional<Variant>(value));
}

void CompoundWrite::AddWriteInline(const Path& path, const Variant& value) {
  AddWriteInline(path, Optional<Variant>(value));
}

void CompoundWrite::AddWriteInline(const std::string& key,
                                   const Optional<Variant>& value) {
  AddWriteInline(Path(key), value);
}

void CompoundWrite::AddWriteInline(const std::string& key,
                                   const Variant& value) {
  AddWriteInline(Path(key), Optional<Variant>(value));
}

CompoundWrite CompoundWrite::AddWrites(const Path& path,
                                       const CompoundWrite& updates) const {
  return updates.write_tree_.Fold(
      *this, [&path](Path relative_path, Variant value, CompoundWrite accum) {
        return accum.AddWrite(path.GetChild(relative_path),
                              Optional<Variant>(value));
      });
}

void CompoundWrite::AddWritesInline(const Path& path,
                                    const CompoundWrite& updates) {
  updates.write_tree_.Fold(
      0, [&path, this](Path relative_path, Variant value, int) {
        AddWriteInline(path.GetChild(relative_path), Optional<Variant>(value));
        return 0;
      });
}

CompoundWrite CompoundWrite::RemoveWrite(const Path& path) const {
  if (path.empty()) {
    return CompoundWrite();
  } else {
    CompoundWrite result(*this);
    Tree<Variant>* subtree = result.write_tree_.GetChild(path);
    if (subtree) {
      subtree->children().clear();
      subtree->value().reset();
    }
    return result;
  }
  return CompoundWrite();
}

void CompoundWrite::RemoveWriteInline(const Path& path) {
  if (path.empty()) {
    *this = CompoundWrite();
  } else {
    Tree<Variant>* subtree = write_tree_.GetChild(path);
    if (subtree) {
      subtree->children().clear();
      subtree->value().reset();
    }
  }
}

bool CompoundWrite::HasCompleteWrite(const Path& path) const {
  return GetCompleteVariant(path).has_value();
}

const Optional<Variant>& CompoundWrite::GetRootWrite() const {
  return write_tree_.value();
}

Optional<Variant> CompoundWrite::GetCompleteVariant(const Path& path) const {
  Optional<Path> root_most = write_tree_.FindRootMostPathWithValue(path);
  if (root_most.has_value()) {
    const Variant* root_most_value = write_tree_.GetValueAt(*root_most);
    Optional<Path> remaining_path = Path::GetRelative(*root_most, path);
    return Optional<Variant>(VariantGetChild(root_most_value, *remaining_path));
  } else {
    return Optional<Variant>();
  }
}

std::vector<std::pair<Variant, Variant>> CompoundWrite::GetCompleteChildren()
    const {
  std::vector<std::pair<Variant, Variant>> children;
  if (GetRootWrite().has_value()) {
    const Variant* value = GetVariantValue(&write_tree_.value().value());
    if (value->is_map()) {
      for (auto& entry : value->map()) {
        children.push_back(entry);
      }
    }
  } else {
    for (auto& entry : write_tree_.children()) {
      const std::string& key = entry.first;
      const Tree<Variant>& subtree = entry.second;
      if (subtree.value().has_value()) {
        children.push_back(std::make_pair(key, subtree.value().value()));
      }
    }
  }
  return children;
}

CompoundWrite CompoundWrite::ChildCompoundWrite(const Path& path) const {
  if (path.empty()) {
    return *this;
  } else {
    Optional<Variant> shadowing_variant = GetCompleteVariant(path);
    if (shadowing_variant.has_value()) {
      return CompoundWrite(Tree<Variant>(shadowing_variant));
    } else {
      // Let the constructor extract the priority update.
      const Tree<Variant>* subtree = write_tree_.GetChild(path);
      return subtree ? CompoundWrite(*subtree) : CompoundWrite();
    }
  }
}

std::map<std::string, CompoundWrite> CompoundWrite::ChildCompoundWrites()
    const {
  std::map<std::string, CompoundWrite> children;
  for (auto& key_subtree_pair : write_tree_.children()) {
    const std::string& key = key_subtree_pair.first;
    const Tree<Variant>& subtree = key_subtree_pair.second;
    children[key] = CompoundWrite(subtree);
  }
  return children;
}

bool CompoundWrite::IsEmpty() const { return write_tree_.IsEmpty(); }

Variant CompoundWrite::Apply(const Variant& variant) const {
  return ApplySubtreeWrite(Path::GetRoot(), &write_tree_, variant);
}

Variant CompoundWrite::ApplySubtreeWrite(const Path& relative_path,
                                         const Tree<Variant>* write_tree,
                                         Variant variant) const {
  if (write_tree->value().has_value()) {
    // Since there a write is always a leaf, we're done here
    VariantUpdateChild(&variant, relative_path, write_tree->value().value());
  } else {
    Optional<Variant> priority_write;
    for (auto& key_tree_pair : write_tree->children()) {
      const std::string& child_key = key_tree_pair.first;
      const Tree<Variant>& child_tree = key_tree_pair.second;
      if (IsPriorityKey(child_key)) {
        // Apply priorities at the end so we don't update priorities for
        // either empty variants or forget to apply priorities to empty
        // variants that are later filled
        FIREBASE_DEV_ASSERT_MESSAGE(
            child_tree.children().empty(),
            "Priority writes must always be leaf variants");
        priority_write = child_tree.value();
      } else {
        variant = ApplySubtreeWrite(relative_path.GetChild(child_key),
                                    &child_tree, variant);
      }
    }
    // If there was a priority write, we only apply it if the variant is not
    // empty.
    if (!VariantIsEmpty(VariantGetChild(&variant, relative_path)) &&
        priority_write.has_value()) {
      VariantUpdateChild(&variant, relative_path.GetChild(kPriorityKey),
                         priority_write.value());
    }
  }
  return variant;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
