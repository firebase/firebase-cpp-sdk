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
#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TREE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TREE_H_

#include <map>
#include <string>

#include "app/src/optional.h"
#include "app/src/path.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

// A very quick and dirty Tree class that has nodes that can hold a value as
// well a map of child nodes.
template <typename Value>
class Tree {
 public:
  Tree() : key_(), value_(), children_(), parent_(nullptr) {}

  Tree(const Tree& other)
      : key_(other.key_),
        value_(other.value_),
        children_(other.children_),
        parent_(nullptr) {
    for (auto& key_subtree_pair : children_) {
      key_subtree_pair.second.parent_ = this;
    }
  }

  Tree& operator=(const Tree& other) {
    key_ = other.key_;
    value_ = other.value_;
    children_ = other.children_;
    parent_ = nullptr;
    for (auto& key_subtree_pair : children_) {
      key_subtree_pair.second.parent_ = this;
    }
    return *this;
  }

  Tree(Tree&& other)
      : key_(std::move(other.key_)),
        value_(std::move(other.value_)),
        children_(std::move(other.children_)),
        parent_(std::move(other.parent_)) {
    for (auto& key_subtree_pair : children_) {
      key_subtree_pair.second.parent_ = this;
    }
  }

  Tree& operator=(Tree&& other) {
    key_ = std::move(other.key_);
    value_ = std::move(other.value_);
    children_ = std::move(other.children_);
    parent_ = std::move(other.parent_);
    for (auto& key_subtree_pair : children_) {
      key_subtree_pair.second.parent_ = this;
    }
    return *this;
  }

  explicit Tree(const Value& value)
      : key_(), value_(value), children_(), parent_(nullptr) {}
  explicit Tree(const Optional<Value>& maybe_value)
      : key_(), value_(maybe_value), children_(), parent_(nullptr) {}
  explicit Tree(Value&& value)
      : key_(), value_(std::move(value)), children_(), parent_(nullptr) {}
  explicit Tree(Optional<Value>&& maybe_value)
      : key_(), value_(std::move(maybe_value)), children_(), parent_(nullptr) {}

  ~Tree() {}

  // Return the key of this node in the tree. Root elements will not have a key.
  const std::string& key() const { return key_; }

  // If a value has been set at this location, return a pointer to it. Return
  // nullptr otherwise. Note that the pointer might go bad if any operations are
  // applied to the tree (such as adding new children).
  Optional<Value>& value() { return value_; }
  const Optional<Value>& value() const { return value_; }

  // Return the map of key/child-nodes.
  std::map<std::string, Tree<Value>>& children() { return children_; }
  const std::map<std::string, Tree<Value>>& children() const {
    return children_;
  }

  // Return a pointer to the parent node of this node in the tree, if present.
  const Tree<Value>* parent() const { return parent_; }

  // Set the value at this location in the tree.
  void set_value(const Value& value) { value_ = value; }
  void set_value(const Optional<Value>& maybe_value) { value_ = maybe_value; }
  void set_value(Value&& value) { value_ = std::move(value); }
  void set_value(Optional<Value>&& maybe_value) {
    value_ = std::move(maybe_value);
  }

  Tree<Value>* GetOrMakeSubtree(const Path& path) {
    Tree<Value>* current_subtree = this;
    for (const std::string& directory : path.GetDirectories()) {
      auto& children = current_subtree->children();
      auto iter = children.find(directory);
      if (iter == children.end()) {
        auto result = children.insert(std::make_pair(directory, Tree<Value>()));
        iter = result.first;
        iter->second.key_ = directory;
        iter->second.parent_ = current_subtree;
      }
      current_subtree = &iter->second;
    }
    return current_subtree;
  }

  // Set the value at a child location in the tree, creating any intermediate
  // nodes as required. If there is already a value there, the value is
  // overwritten. And empty path writes to the current node. Returns a pointer
  // to the value that was just set.
  Optional<Value>& SetValueAt(const Path& path, const Optional<Value>& value) {
    Tree<Value>* current_subtree = GetOrMakeSubtree(path);
    current_subtree->set_value(value);
    return current_subtree->value();
  }

  Optional<Value>& SetValueAt(const Path& path, Optional<Value>&& value) {
    Tree<Value>* current_subtree = GetOrMakeSubtree(path);
    current_subtree->set_value(std::move(value));
    return current_subtree->value();
  }

  Optional<Value>& SetValueAt(const Path& path, const Value& value) {
    return SetValueAt(path, Optional<Value>(value));
  }

  Optional<Value>& SetValueAt(const Path& path, Value&& value) {
    return SetValueAt(path, Optional<Value>(std::move(value)));
  }

  // Returns the root-most element in the tree in the given path. For example,
  // if a Tree<int> contains the following values:
  //
  //  -+ <root>: None
  //   |
  //   +- "Foo": None
  //   |  |
  //   |  +- "Bar": 100
  //   |     |
  //   |     +- "Baz": 200
  //   |
  //   +- "quux": 300
  //
  // And the path given is "foo/bar/baz", then the return value will be a
  // pointer to 100, because that is the root-most value in the given path. If
  // a value cannot be found then nullptr is returned.
  const Value* RootMostValue(const Path& path) const {
    return RootMostValueMatching(path, [](const Value& value) { return true; });
  }

  // Returns the root-most element in the tree in the given path that matches
  // the given predicate. For example, if a Tree<int> contains the following
  // values:
  //
  //  -+ <root>: 0
  //   |
  //   +- "Foo": 50
  //   |  |
  //   |  +- "Bar": 100
  //   |     |
  //   |     +- "Baz": 200
  //   |
  //   +- "quux": 300
  //
  // And the path given is "foo/bar/baz", and the predicate is value > 75, then
  // the return value will be a pointer to 100, because that is the root-most
  // value in the given path that meets the condition given.
  template <typename Func>
  const Value* RootMostValueMatching(const Path& path,
                                     const Func& predicate) const {
    if (value_.has_value() && predicate(*value_)) {
      return &value_.value();
    } else {
      const Tree<Value>* current_tree = this;
      for (const std::string& directory : path.GetDirectories()) {
        current_tree = current_tree->GetChild(directory);
        if (current_tree == nullptr) {
          return nullptr;
        } else if (current_tree->value_.has_value() &&
                   predicate(*current_tree->value_)) {
          return &current_tree->value_.value();
        }
      }
      return nullptr;
    }
  }

  // Returns the leaf-most element in the tree in the given path. For example,
  // if a Tree<int> contains the following values:
  //
  //  -+ <root>: None
  //   |
  //   +- "Foo": 200
  //   |  |
  //   |  +- "Bar": 100
  //   |     |
  //   |     +- "Baz": None
  //   |
  //   +- "quux": 300
  //
  // And the path given is "foo/bar/baz", then the return value will be a
  // pointer to 100, because that is the leaf-most value in the given path. If
  // a value cannot be found then nullptr is returned.
  const Value* LeafMostValue(const Path& path) const {
    return LeafMostValueMatching(path, [](const Value& value) { return true; });
  }

  // Returns the leaf-most element in the tree in the given path that matches
  // the given predicate. For example, if a Tree<int> contains the following
  // values:
  //
  //  -+ <root>: 200
  //   |
  //   +- "Foo": 100
  //   |  |
  //   |  +- "Bar": 50
  //   |     |
  //   |     +- "Baz": 0
  //   |
  //   +- "quux": 300
  //
  // And the path given is "foo/bar/baz", and the predicate is value > 75, then
  // the return value will be a pointer to 100, because that is the leaf-most
  // value in the given path that meets the condition given.
  template <typename Func>
  const Value* LeafMostValueMatching(const Path& path,
                                     const Func& predicate) const {
    const Value* current_value =
        (value_.has_value() && predicate(*value_)) ? &value_.value() : nullptr;
    const Tree<Value>* current_tree = this;
    for (const std::string& directory : path.GetDirectories()) {
      current_tree = current_tree->GetChild(directory);
      if (current_tree == nullptr) {
        return current_value;
      } else {
        if (current_tree->value_.has_value() &&
            predicate(*current_tree->value_)) {
          current_value = &current_tree->value_.value();
        }
      }
    }
    return current_value;
  }

  // Returns true if any location at or beneath this location in the tree meets
  // the criteria given by the predicate.
  template <typename Func>
  bool ContainsMatchingValue(const Func& predicate) const {
    if (value_.has_value() && predicate(*value_)) {
      return true;
    } else {
      for (auto& key_subtree_pair : children_) {
        const Tree<Value>& subtree = key_subtree_pair.second;
        if (subtree.ContainsMatchingValue(predicate)) {
          return true;
        }
      }
      return false;
    }
  }

  // Get a child node using the given key.
  Tree<Value>* GetChild(const std::string& key) {
    if (key.empty()) {
      return this;
    }
    return MapGet(&children_, key);
  }

  // Get a child node using the given key.
  const Tree<Value>* GetChild(const std::string& key) const {
    if (key.empty()) {
      return this;
    }
    return MapGet(&children_, key);
  }

  // Get a child node using the given path. If there is no node at the given
  // path, nullptr is returned.
  Tree<Value>* GetChild(const Path& path) {
    Tree<Value>* result = this;
    for (const std::string& directory : path.GetDirectories()) {
      Tree<Value>* child = result->GetChild(directory);
      if (child == nullptr) {
        return nullptr;
      }
      result = child;
    }
    return result;
  }

  // Get a child node using the given path. If there is no node at the given
  // path, nullptr is returned.
  const Tree<Value>* GetChild(const Path& path) const {
    return const_cast<Tree<Value>*>(this)->GetChild(path);
  }

  // Returns the value in the tree at the given path, if present. If either the
  // tree does not have a node at the given location, or that node is present
  // but has no value, this will return nullptr. The value returned will only
  // remain valid while the tree is valid and unmodified.
  Value* GetValueAt(const Path& path) {
    Tree<Value>* child = GetChild(path);
    if (!child) return nullptr;
    Optional<Value>& maybe_value = child->value();
    if (!maybe_value.has_value()) return nullptr;
    return &maybe_value.value();
  }

  const Value* GetValueAt(const Path& path) const {
    const Tree<Value>* child = GetChild(path);
    if (!child) return nullptr;
    const Optional<Value>& maybe_value = child->value();
    if (!maybe_value.has_value()) return nullptr;
    return &maybe_value.value();
  }

  // Return true if there is no value and no child nodes at this location in the
  // tree.
  bool IsEmpty() const { return !value_.has_value() && children_.empty(); }

  // Get the full path to this element in the tree from the root.
  Path GetPath() const {
    if (parent_) {
      return parent_->GetPath().GetChild(key_);
    } else {
      return Path(key_);
    }
  }

  // Call a function on each present value in the tree in pre-order, starting
  // from the given path.
  // The function can be std::function or a non-class function pointer.
  // Expected parameters:
  //   param1: Path or const Path&
  //   param2: Value, Value& or const Value&
  // Ex. foo(const Path& path, Value& value);
  // The function should not mutate children_ in any subtree or the iterator may
  // get corrupted and crash.
  template <typename Func>
  void CallOnEach(const Path& path, const Func& func) {
    Tree<Value>* subtree = GetChild(path);
    if (subtree) {
      subtree->CallOnEachInternal(path, func);
    }
  }

  template <typename Func>
  void CallOnEach(const Path& path, const Func& func) const {
    const_cast<Tree<Value>*>(this)->CallOnEach(path, func);
  }

  // Call a function on each present value in the tree in pre-order, starting
  // from the given path.
  // The function can only be a non-class function pointer.
  // The function should not mutate children_ in any subtree or the iterator may
  // get corrupted and crash.
  typedef void (*CallFunc)(const Path&, Value*, void*);

  // TODO(amablue): Replace calls to this with Fold.
  void CallOnEach(const Path& path, CallFunc func, void* data) {
    Tree<Value>* subtree = GetChild(path);
    if (subtree) {
      subtree->CallOnEachInternal(path, func, data);
    }
  }

  void CallOnEach(const Path& path, CallFunc func, void* data) const {
    const_cast<Tree<Value>*>(this)->CallOnEach(path, func, data);
  }

  // Call `predicate` on each ancestor of this location in the tree, optionally
  // includeing this location.
  // The predicate can return true to cease futher calls, or false to continue.
  template <typename Func>
  bool CallOnEachAncestor(const Func& predicate, bool include_self) {
    Tree<Value>* tree = include_self ? this : this->parent_;
    for (; tree != nullptr; tree = tree->parent_) {
      if (predicate(tree)) {
        return true;
      }
    }
    return false;
  }

  // Call `predicate` on each ancestor of this location in the tree, not
  // includeing this location.
  // The predicate can return true to cease futher calls, or false to continue.
  template <typename Func>
  bool CallOnEachAncestor(const Func& predicate) {
    return CallOnEachAncestor(predicate, false);
  }

  // Call `predicate` on each descendant of this location in the tree, not
  // including this location.
  // The predicate can return true to cease futher calls, or false to continue.
  template <typename Func>
  void CallOnEachDescendant(const Func& predicate) {
    CallOnEachDescendant(predicate, false, false);
  }

  // Call `predicate` on each descendant of this location in the tree,
  // optionally including this location. This will call the predicate on a given
  // location before recursing deeper into the tree.
  // The predicate can return true to cease futher calls, or false to continue.
  template <typename Func>
  void CallOnEachDescendant(const Func& predicate, bool include_self) {
    CallOnEachDescendant(predicate, include_self, false);
  }

  // Call `predicate` on each descendant of this location in the tree,
  // optionally including this location. When recursing, you can optionally call
  // the call the predicate on the children first and then recurse, or first
  // recurse and call the children after the recursive step.
  // The predicate can return true to cease futher calls, or false to continue.
  template <typename Func>
  void CallOnEachDescendant(const Func& predicate, bool include_self,
                            bool children_first) {
    if (include_self && !children_first) {
      predicate(this);
    }

    for (auto& key_subtree_pair : children_) {
      Tree<Value>& subtree = key_subtree_pair.second;
      subtree.CallOnEachDescendant(predicate, true, children_first);
    }

    if (include_self && children_first) {
      predicate(this);
    }
  }

  // Given a path, find the root-most element in the tree's path for which the
  // predciate returns true. Only elements that have values are considered.
  template <typename Func>
  Optional<Path> FindRootMostMatchingPath(const Path& path,
                                          const Func& predicate) const {
    std::vector<std::string> directories = path.GetDirectories();
    for (auto iter = directories.begin(); /* see below for break */; ++iter) {
      Path current_path(directories.begin(), iter);
      const Tree<Value>* subtree = GetChild(current_path);
      if (subtree == nullptr) {
        break;
      }
      if (subtree->value().has_value() && predicate(subtree->value().value())) {
        return Optional<Path>(current_path);
      }
      if (iter == directories.end()) {
        // Only break after the loop has executed at least once.
        break;
      }
    }
    return Optional<Path>();
  }

  // Finds the path to the root most tree node that contains a value.
  Optional<Path> FindRootMostPathWithValue(const Path& relative_path) const {
    return FindRootMostMatchingPath(relative_path,
                                    [](const Value& value) { return true; });
  }

  // https://en.wikipedia.org/wiki/Fold_(higher-order_function)
  //
  // Recursively apply a function to each node in the tree containing a value,
  // and accumulate the result of the calls.
  template <typename Accum, typename Func>
  Accum Fold(Accum accum, const Func& visitor) const {
    return Fold(Path(), visitor, accum);
  }

  template <typename Accum, typename Func>
  Accum Fold(const Path& relative_path, Func visitor, Accum accum) const {
    for (auto& key_subtree_pair : children_) {
      const std::string& key = key_subtree_pair.first;
      const Tree<Value>& subtree = key_subtree_pair.second;
      accum = subtree.Fold(relative_path.GetChild(key), visitor, accum);
    }
    if (value_.has_value()) {
      accum = visitor(relative_path, value_.value(), accum);
    }
    return accum;
  }

  bool operator==(const Tree& other) const {
    return value_ == other.value_ && children_ == other.children_;
  }

  bool operator!=(const Tree& other) const { return !(*this == other); }

 private:
  template <typename Func>
  void CallOnEachInternal(const Path& path, const Func& func) {
    if (value_.has_value()) {
      func(path, value_.value());
    }
    for (auto& key_subtree_pair : children_) {
      const std::string& key = key_subtree_pair.first;
      Tree<Value>& subtree = key_subtree_pair.second;
      subtree.CallOnEachInternal(path.GetChild(key), func);
    }
  }

  void CallOnEachInternal(const Path& path, CallFunc func, void* data) {
    if (value_.has_value()) {
      func(path, &value_.value(), data);
    }
    for (auto& key_subtree_pair : children_) {
      const std::string& key = key_subtree_pair.first;
      Tree<Value>& subtree = key_subtree_pair.second;
      subtree.CallOnEachInternal(path.GetChild(key), func, data);
    }
  }

  // The key of this element in the Tree. This will the eqivalent to the
  // std::string stored in the children_ map in the parent Tree node.
  std::string key_;

  // The value stored at this location in the tree. A location in a tree is not
  // required to hold a value.
  Optional<Value> value_;

  // The child nodes.
  std::map<std::string, Tree<Value>> children_;

  // The parent node. This will be a nullptr on root nodes.
  Tree<Value>* parent_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TREE_H_
