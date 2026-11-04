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
#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CORE_TREE_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CORE_TREE_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/src/path.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

template <typename Value>
class Tree {
 public:
  Tree() = default;

  Tree(const Tree& other)
      : key_(other.key_),
        value_(other.value_),
        children_(other.children_),
        parent_(nullptr) {
    UpdateParentPointers();
  }

  Tree& operator=(const Tree& other) {
    if (this != &other) {
      key_ = other.key_;
      value_ = other.value_;
      children_ = other.children_;
      parent_ = nullptr;
      UpdateParentPointers();
    }
    return *this;
  }

  Tree(Tree&& other) noexcept
      : key_(std::move(other.key_)),
        value_(std::move(other.value_)),
        children_(std::move(other.children_)),
        parent_(other.parent_) {
    other.value_.reset();
    other.parent_ = nullptr;
    UpdateParentPointers();
  }

  Tree& operator=(Tree&& other) noexcept {
    if (this != &other) {
      key_ = std::move(other.key_);
      value_ = std::move(other.value_);
      other.value_.reset();
      children_ = std::move(other.children_);
      parent_ = other.parent_;
      other.parent_ = nullptr;
      UpdateParentPointers();
    }
    return *this;
  }

  explicit Tree(const Value& value) : value_(value) {}
  explicit Tree(const std::optional<Value>& maybe_value)
      : value_(maybe_value) {}
  explicit Tree(Value&& value) : value_(std::move(value)) {}
  explicit Tree(std::optional<Value>&& maybe_value)
      : value_(std::move(maybe_value)) {}

  ~Tree() = default;

  const std::string& key() const { return key_; }
  std::optional<Value>& value() { return value_; }
  const std::optional<Value>& value() const { return value_; }
  std::map<std::string, Tree<Value>>& children() { return children_; }
  const std::map<std::string, Tree<Value>>& children() const {
    return children_;
  }
  const Tree<Value>* parent() const { return parent_; }

  void set_value(const Value& value) { value_ = value; }
  void set_value(const std::optional<Value>& maybe_value) {
    value_ = maybe_value;
  }
  void set_value(Value&& value) { value_ = std::move(value); }
  void set_value(std::optional<Value>&& maybe_value) {
    value_ = std::move(maybe_value);
  }

  Tree<Value>* GetOrMakeSubtree(const Path& path) {
    Tree<Value>* current = this;
    for (const std::string& directory : path.GetDirectories()) {
      auto [iter, inserted] =
          current->children_.try_emplace(directory, Tree<Value>());
      if (inserted) {
        iter->second.key_ = directory;
        iter->second.parent_ = current;
      }
      current = &iter->second;
    }
    return current;
  }

  std::optional<Value>& SetValueAt(const Path& path,
                                   const std::optional<Value>& value) {
    Tree<Value>* subtree = GetOrMakeSubtree(path);
    subtree->set_value(value);
    return subtree->value();
  }

  std::optional<Value>& SetValueAt(const Path& path,
                                   std::optional<Value>&& value) {
    Tree<Value>* subtree = GetOrMakeSubtree(path);
    subtree->set_value(std::move(value));
    return subtree->value();
  }

  std::optional<Value>& SetValueAt(const Path& path, const Value& value) {
    return SetValueAt(path, std::optional<Value>(value));
  }

  std::optional<Value>& SetValueAt(const Path& path, Value&& value) {
    return SetValueAt(path, std::optional<Value>(std::move(value)));
  }

  const Value* RootMostValue(const Path& path) const {
    return RootMostValueMatching(path, [](const Value&) { return true; });
  }

  template <typename Func>
  const Value* RootMostValueMatching(const Path& path,
                                     const Func& predicate) const {
    if (value_.has_value() && predicate(*value_)) return &value_.value();
    const Tree<Value>* current = this;
    for (const std::string& directory : path.GetDirectories()) {
      current = current->GetChild(directory);
      if (current == nullptr) return nullptr;
      if (current->value_.has_value() && predicate(*current->value_)) {
        return &current->value_.value();
      }
    }
    return nullptr;
  }

  const Value* LeafMostValue(const Path& path) const {
    return LeafMostValueMatching(path, [](const Value&) { return true; });
  }

  template <typename Func>
  const Value* LeafMostValueMatching(const Path& path,
                                     const Func& predicate) const {
    const Value* result =
        (value_.has_value() && predicate(*value_)) ? &value_.value() : nullptr;
    const Tree<Value>* current = this;
    for (const std::string& directory : path.GetDirectories()) {
      current = current->GetChild(directory);
      if (current == nullptr) return result;
      if (current->value_.has_value() && predicate(*current->value_)) {
        result = &current->value_.value();
      }
    }
    return result;
  }

  template <typename Func>
  bool ContainsMatchingValue(const Func& predicate) const {
    if (value_.has_value() && predicate(*value_)) return true;
    for (const auto& [key, subtree] : children_) {
      if (subtree.ContainsMatchingValue(predicate)) return true;
    }
    return false;
  }

  Tree<Value>* GetChild(const std::string& key) {
    return key.empty() ? this : MapGet(&children_, key);
  }

  const Tree<Value>* GetChild(const std::string& key) const {
    return key.empty() ? this : MapGet(&children_, key);
  }

  Tree<Value>* GetChild(const Path& path) {
    Tree<Value>* result = this;
    for (const std::string& directory : path.GetDirectories()) {
      result = result->GetChild(directory);
      if (result == nullptr) return nullptr;
    }
    return result;
  }

  const Tree<Value>* GetChild(const Path& path) const {
    return const_cast<Tree<Value>*>(this)->GetChild(path);
  }

  Value* GetValueAt(const Path& path) {
    Tree<Value>* child = GetChild(path);
    return (child && child->value_.has_value()) ? &child->value_.value()
                                                : nullptr;
  }

  const Value* GetValueAt(const Path& path) const {
    return const_cast<Tree<Value>*>(this)->GetValueAt(path);
  }

  bool IsEmpty() const { return !value_.has_value() && children_.empty(); }

  Path GetPath() const {
    return parent_ ? parent_->GetPath().GetChild(key_) : Path(key_);
  }

  template <typename Func>
  void CallOnEach(const Path& path, const Func& func) {
    if (Tree<Value>* subtree = GetChild(path)) {
      subtree->CallOnEachInternal(path, func);
    }
  }

  template <typename Func>
  void CallOnEach(const Path& path, const Func& func) const {
    const_cast<Tree<Value>*>(this)->CallOnEach(path, func);
  }

  using CallFunc = void (*)(const Path&, Value*, void*);

  void CallOnEach(const Path& path, CallFunc func, void* data) {
    if (Tree<Value>* subtree = GetChild(path)) {
      subtree->CallOnEachInternal(path, func, data);
    }
  }

  void CallOnEach(const Path& path, CallFunc func, void* data) const {
    const_cast<Tree<Value>*>(this)->CallOnEach(path, func, data);
  }

  template <typename Func>
  bool CallOnEachAncestor(const Func& predicate, bool include_self = false) {
    for (Tree<Value>* tree = include_self ? this : parent_; tree != nullptr;
         tree = tree->parent_) {
      if (predicate(tree)) return true;
    }
    return false;
  }

  template <typename Func>
  void CallOnEachDescendant(const Func& predicate, bool include_self = false,
                            bool children_first = false) {
    if (include_self && !children_first) predicate(this);
    for (auto& [key, subtree] : children_) {
      subtree.CallOnEachDescendant(predicate, true, children_first);
    }
    if (include_self && children_first) predicate(this);
  }

  template <typename Func>
  std::optional<Path> FindRootMostMatchingPath(const Path& path,
                                               const Func& predicate) const {
    std::vector<std::string> directories = path.GetDirectories();
    for (auto iter = directories.begin();; ++iter) {
      Path current_path(directories.begin(), iter);
      const Tree<Value>* subtree = GetChild(current_path);
      if (subtree == nullptr) break;
      if (subtree->value_.has_value() && predicate(*subtree->value_)) {
        return current_path;
      }
      if (iter == directories.end()) break;
    }
    return std::nullopt;
  }

  std::optional<Path> FindRootMostPathWithValue(
      const Path& relative_path) const {
    return FindRootMostMatchingPath(relative_path,
                                    [](const Value&) { return true; });
  }

  template <typename Accum, typename Func>
  Accum Fold(Accum accum, const Func& visitor) const {
    return Fold(Path(), visitor, accum);
  }

  template <typename Accum, typename Func>
  Accum Fold(const Path& relative_path, Func visitor, Accum accum) const {
    for (const auto& [key, subtree] : children_) {
      accum = subtree.Fold(relative_path.GetChild(key), visitor, accum);
    }
    if (value_.has_value()) {
      accum = visitor(relative_path, *value_, accum);
    }
    return accum;
  }

  bool operator==(const Tree& other) const {
    return value_ == other.value_ && children_ == other.children_;
  }
  bool operator!=(const Tree& other) const { return !(*this == other); }

 private:
  void UpdateParentPointers() {
    for (auto& [key, subtree] : children_) subtree.parent_ = this;
  }

  template <typename Func>
  void CallOnEachInternal(const Path& path, const Func& func) {
    if (value_.has_value()) func(path, *value_);
    for (auto& [key, subtree] : children_) {
      subtree.CallOnEachInternal(path.GetChild(key), func);
    }
  }

  void CallOnEachInternal(const Path& path, CallFunc func, void* data) {
    if (value_.has_value()) func(path, &value_.value(), data);
    for (auto& [key, subtree] : children_) {
      subtree.CallOnEachInternal(path.GetChild(key), func, data);
    }
  }

  std::string key_;
  std::optional<Value> value_;
  std::map<std::string, Tree<Value>> children_;
  Tree<Value>* parent_ = nullptr;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CORE_TREE_H_
