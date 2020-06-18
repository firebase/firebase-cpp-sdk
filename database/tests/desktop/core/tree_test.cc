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

#include "database/src/desktop/core/tree.h"

#include "app/memory/unique_ptr.h"
#include "app/src/optional.h"
#include "app/src/path.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {

namespace {

using ::testing::Eq;

typedef std::pair<int, int> IntPair;

TEST(TreeTest, DefaultConstruct) {
  {
    Tree<int> tree;
    EXPECT_FALSE(tree.value().has_value());
    EXPECT_EQ(tree.children().size(), 0);
  }

  {
    Tree<int> tree(1);
    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 1);
    EXPECT_EQ(tree.children().size(), 0);
  }
}

TEST(TreeTest, CopyConstructor) {
  Tree<int> source(1234);
  source.SetValueAt(Path("aaa/bbb/ccc"), 5678);
  Tree<int> destination(source);

  // Ensure values got copied correctly.
  Tree<int>* subtree = destination.GetChild(Path("aaa/bbb/ccc"));
  EXPECT_EQ(*destination.value(), 1234);
  EXPECT_EQ(*subtree->value(), 5678);
  EXPECT_EQ(subtree->GetPath(), Path("aaa/bbb/ccc"));

  // Ensure source is still populated.
  subtree = source.GetChild(Path("aaa/bbb/ccc"));
  EXPECT_EQ(*source.value(), 1234);
  EXPECT_EQ(*subtree->value(), 5678);
  EXPECT_EQ(subtree->GetPath(), Path("aaa/bbb/ccc"));
}

TEST(TreeTest, CopyAssignment) {
  Tree<int> source(1234);
  source.SetValueAt(Path("aaa/bbb/ccc"), 5678);
  Tree<int> destination(-9999);
  destination.SetValueAt(Path("zzz/yyy/xxx"), -9999);

  destination = source;

  // Ensure values got copied correctly.
  Tree<int>* subtree = destination.GetChild(Path("aaa/bbb/ccc"));
  EXPECT_EQ(*destination.value(), 1234);
  EXPECT_EQ(*subtree->value(), 5678);
  EXPECT_EQ(subtree->GetPath(), Path("aaa/bbb/ccc"));

  // Ensure old values were not left behind.
  Tree<int>* bad_subtree = destination.GetChild(Path("zzz/yyy/xxx"));
  EXPECT_EQ(bad_subtree, nullptr);

  // Ensure source is still populated.
  subtree = source.GetChild(Path("aaa/bbb/ccc"));
  EXPECT_EQ(*source.value(), 1234);
  EXPECT_EQ(*subtree->value(), 5678);
  EXPECT_EQ(subtree->GetPath(), Path("aaa/bbb/ccc"));
}

TEST(TreeTest, MoveConstructor) {
  Tree<int> source(1234);
  source.SetValueAt(Path("aaa/bbb/ccc"), 5678);
  Tree<int> destination(std::move(source));

  // Ensure values got copied correctly.
  Tree<int>* subtree = destination.GetChild(Path("aaa/bbb/ccc"));
  EXPECT_EQ(*destination.value(), 1234);
  EXPECT_EQ(*subtree->value(), 5678);
  EXPECT_EQ(subtree->GetPath(), Path("aaa/bbb/ccc"));

  // Ensure source is empty.
  EXPECT_FALSE(source.value().has_value());  // NOLINT
  EXPECT_TRUE(source.children().empty());    // NOLINT
}

TEST(TreeTest, MoveAssignment) {
  Tree<int> source(1234);
  source.SetValueAt(Path("aaa/bbb/ccc"), 5678);
  Tree<int> destination(-9999);
  destination.SetValueAt(Path("zzz/yyy/xxx"), -9999);

  destination = std::move(source);

  // Ensure values got copied correctly.
  Tree<int>* subtree = destination.GetChild(Path("aaa/bbb/ccc"));
  EXPECT_EQ(*destination.value(), 1234);
  EXPECT_EQ(*subtree->value(), 5678);
  EXPECT_EQ(subtree->GetPath(), Path("aaa/bbb/ccc"));

  // Ensure old values were not left behind.
  Tree<int>* bad_subtree = destination.GetChild(Path("zzz/yyy/xxx"));
  EXPECT_EQ(bad_subtree, nullptr);

  // Ensure source is empty.
  EXPECT_FALSE(source.value().has_value());  // NOLINT
  EXPECT_TRUE(source.children().empty());    // NOLINT
}

TEST(TreeTest, GetSetValue) {
  {
    Tree<int> tree(1);
    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 1);

    tree.set_value(2);
    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 2);
  }
}

TEST(TreeTest, GetSetRValue) {
  {
    Tree<UniquePtr<int>> tree(MakeUnique<int>(1));
    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(*tree.value().value(), 1);

    tree.set_value(MakeUnique<int>(2));
    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(*tree.value().value(), 2);
  }
}

TEST(TreeTest, GetValueAt) {
  {
    Tree<int> tree;

    int* root = tree.GetValueAt(Path(""));
    EXPECT_EQ(root, nullptr);
    EXPECT_EQ(tree.GetValueAt(Path("A")), nullptr);
  }

  {
    Tree<int> tree(1);

    int* root = tree.GetValueAt(Path(""));
    EXPECT_NE(root, nullptr);
    EXPECT_EQ(*root, 1);
    EXPECT_EQ(tree.GetValueAt(Path("A")), nullptr);
  }

  {
    Tree<int> tree(1);
    tree.children()["A"].set_value(2);
    tree.children()["B"].set_value(3);

    int* root = tree.GetValueAt(Path(""));
    EXPECT_NE(root, nullptr);
    EXPECT_EQ(*root, 1);

    int* child_a = tree.GetValueAt(Path("A"));
    EXPECT_NE(child_a, nullptr);
    EXPECT_EQ(*child_a, 2);

    int* child_b = tree.GetValueAt(Path("B"));
    EXPECT_NE(child_b, nullptr);
    EXPECT_EQ(*child_b, 3);
  }

  {
    Tree<int> tree(1);
    tree.children()["A"].set_value(2);
    tree.children()["A"].children()["A1"].set_value(20);
    tree.children()["B"].children()["B1"].set_value(30);

    int* root = tree.GetValueAt(Path(""));
    EXPECT_NE(root, nullptr);
    EXPECT_EQ(*root, 1);

    int* child_a = tree.GetValueAt(Path("A"));
    EXPECT_NE(child_a, nullptr);
    EXPECT_EQ(*child_a, 2);

    int* child_a_a1 = tree.GetValueAt(Path("A/A1"));
    EXPECT_NE(child_a_a1, nullptr);
    EXPECT_EQ(*child_a_a1, 20);

    int* child_b = tree.GetValueAt(Path("B"));
    EXPECT_EQ(child_b, nullptr);

    int* child_b_b1 = tree.GetValueAt(Path("B/B1"));
    EXPECT_NE(child_b_b1, nullptr);
    EXPECT_EQ(*child_b_b1, 30);
  }
}

TEST(TreeTest, SetValueAt) {
  {
    Tree<int> tree;
    tree.SetValueAt(Path(""), 1);

    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 1);
    EXPECT_EQ(tree.children().size(), 0);
  }

  {
    Tree<int> tree(1);
    tree.SetValueAt(Path("A"), 2);
    tree.SetValueAt(Path("B"), 3);

    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 1);
    EXPECT_EQ(tree.children().size(), 2);

    int* child_a = tree.GetValueAt(Path("A"));
    EXPECT_NE(child_a, nullptr);
    EXPECT_EQ(*child_a, 2);

    int* child_b = tree.GetValueAt(Path("B"));
    EXPECT_NE(child_b, nullptr);
    EXPECT_EQ(*child_b, 3);
  }

  {
    Tree<int> tree(1);
    tree.SetValueAt(Path("A"), 2);
    tree.SetValueAt(Path("A/A1"), 20);
    tree.SetValueAt(Path("B/B1"), 30);

    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 1);
    EXPECT_EQ(tree.children().size(), 2);

    int* child_a = tree.GetValueAt(Path("A"));
    EXPECT_NE(child_a, nullptr);
    EXPECT_EQ(*child_a, 2);

    int* child_a_a1 = tree.GetValueAt(Path("A/A1"));
    EXPECT_NE(child_a_a1, nullptr);
    EXPECT_EQ(*child_a_a1, 20);

    int* child_b = tree.GetValueAt(Path("B"));
    EXPECT_EQ(child_b, nullptr);

    int* child_b_b1 = tree.GetValueAt(Path("B/B1"));
    EXPECT_NE(child_b_b1, nullptr);
    EXPECT_EQ(*child_b_b1, 30);
  }
}

TEST(TreeTest, SetValueAtRValue) {
  {
    Tree<UniquePtr<int>> tree;
    tree.SetValueAt(Path(""), MakeUnique<int>(1));

    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 1);
    EXPECT_EQ(tree.children().size(), 0);
  }

  {
    Tree<UniquePtr<int>> tree(MakeUnique<int>(1));
    tree.SetValueAt(Path("A"), MakeUnique<int>(2));
    tree.SetValueAt(Path("B"), MakeUnique<int>(3));

    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 1);
    EXPECT_EQ(tree.children().size(), 2);

    UniquePtr<int>* child_a = tree.GetValueAt(Path("A"));
    EXPECT_NE(child_a, nullptr);
    EXPECT_EQ(**child_a, 2);

    UniquePtr<int>* child_b = tree.GetValueAt(Path("B"));
    EXPECT_NE(child_b, nullptr);
    EXPECT_EQ(**child_b, 3);
  }

  {
    Tree<UniquePtr<int>> tree(MakeUnique<int>(1));
    tree.SetValueAt(Path("A"), MakeUnique<int>(2));
    tree.SetValueAt(Path("A/A1"), MakeUnique<int>(20));
    tree.SetValueAt(Path("B/B1"), MakeUnique<int>(30));

    EXPECT_TRUE(tree.value().has_value());
    EXPECT_EQ(tree.value().value(), 1);
    EXPECT_EQ(tree.children().size(), 2);

    UniquePtr<int>* child_a = tree.GetValueAt(Path("A"));
    EXPECT_NE(child_a, nullptr);
    EXPECT_EQ(**child_a, 2);

    UniquePtr<int>* child_a_a1 = tree.GetValueAt(Path("A/A1"));
    EXPECT_NE(child_a_a1, nullptr);
    EXPECT_EQ(**child_a_a1, 20);

    UniquePtr<int>* child_b = tree.GetValueAt(Path("B"));
    EXPECT_EQ(child_b, nullptr);

    UniquePtr<int>* child_b_b1 = tree.GetValueAt(Path("B/B1"));
    EXPECT_NE(child_b_b1, nullptr);
    EXPECT_EQ(**child_b_b1, 30);
  }
}

TEST(TreeTest, RootMostValue) {
  {
    Tree<IntPair> tree({1, 2});
    tree.SetValueAt(Path("A"), {3, 4});
    tree.SetValueAt(Path("A/B"), {5, 6});
    tree.SetValueAt(Path("A/B/C"), {7, 8});
    tree.SetValueAt(Path("A/B/D"), {9, 10});
    tree.SetValueAt(Path("A/B/D"), {1, 9999});
    EXPECT_EQ(*tree.RootMostValue(Path()), std::make_pair(1, 2));
    EXPECT_EQ(*tree.RootMostValue(Path("A")), std::make_pair(1, 2));
    EXPECT_EQ(*tree.RootMostValue(Path("B")), std::make_pair(1, 2));
  }
  {
    Tree<IntPair> tree;
    tree.SetValueAt(Path("A/B"), {5, 6});
    tree.SetValueAt(Path("Z/Z"), {5, -9999});
    tree.SetValueAt(Path("A/B/C"), {7, 8});
    tree.SetValueAt(Path("A/B/D"), {9, 10});
    EXPECT_EQ(tree.RootMostValue(Path()), nullptr);
    EXPECT_EQ(tree.RootMostValue(Path("A")), nullptr);
    EXPECT_EQ(tree.RootMostValue(Path("B")), nullptr);
    EXPECT_EQ(*tree.RootMostValue(Path("A/B")), std::make_pair(5, 6));
    EXPECT_EQ(*tree.RootMostValue(Path("A/B/C")), std::make_pair(5, 6));
  }
  {
    Tree<IntPair> tree;
    EXPECT_EQ(tree.RootMostValue(Path()), nullptr);
  }
}

TEST(TreeTest, RootMostValueMatching) {
  auto find_three = [](const IntPair& value) { return value.first == 3; };
  {
    Tree<IntPair> tree({1, 2});
    tree.SetValueAt(Path("A"), {3, 4});
    tree.SetValueAt(Path("A/B"), {5, 6});
    tree.SetValueAt(Path("A/B/C"), {3, -9999});
    tree.SetValueAt(Path("A/B/D"), {9, 10});
    EXPECT_EQ(tree.RootMostValueMatching(Path(), find_three), nullptr);
    EXPECT_EQ(*tree.RootMostValueMatching(Path("A"), find_three),
              std::make_pair(3, 4));
    EXPECT_EQ(*tree.RootMostValueMatching(Path("A/B/C"), find_three),
              std::make_pair(3, 4));
    EXPECT_EQ(tree.RootMostValueMatching(Path("B"), find_three), nullptr);
  }
  {
    Tree<IntPair> tree;
    EXPECT_EQ(tree.RootMostValueMatching(Path(), find_three), nullptr);
  }
}

TEST(TreeTest, LeafMostValue) {
  {
    Tree<IntPair> tree({1, 2});
    tree.SetValueAt(Path("A"), {1, 3});
    tree.SetValueAt(Path("A/B"), {1, 4});
    tree.SetValueAt(Path("A/B/C"), {1, 5});
    tree.SetValueAt(Path("A/B/D"), {1, 6});
    EXPECT_EQ(*tree.LeafMostValue(Path()), std::make_pair(1, 2));
    EXPECT_EQ(*tree.LeafMostValue(Path("A")), std::make_pair(1, 3));
    EXPECT_EQ(*tree.LeafMostValue(Path("A/B")), std::make_pair(1, 4));
    EXPECT_EQ(*tree.LeafMostValue(Path("A/B/C")), std::make_pair(1, 5));
    EXPECT_EQ(*tree.LeafMostValue(Path("A/B/C/D")), std::make_pair(1, 5));
    EXPECT_EQ(*tree.LeafMostValue(Path("B")), std::make_pair(1, 2));
  }
  {
    Tree<IntPair> tree;
    EXPECT_EQ(tree.LeafMostValue(Path()), nullptr);
  }
}

TEST(TreeTest, LeafMostValueMatching) {
  {
    auto find_one = [](const IntPair& value) { return value.first == 1; };
    Tree<IntPair> tree({1, 2});
    tree.SetValueAt(Path("A"), {1, 3});
    tree.SetValueAt(Path("A/B"), {1, 4});
    tree.SetValueAt(Path("A/B/C"), {1, 5});
    tree.SetValueAt(Path("A/B/D"), {1, 6});
    EXPECT_EQ(*tree.LeafMostValueMatching(Path(), find_one),
              std::make_pair(1, 2));
    EXPECT_EQ(*tree.LeafMostValueMatching(Path("A"), find_one),
              std::make_pair(1, 3));
    EXPECT_EQ(*tree.LeafMostValueMatching(Path("A/B"), find_one),
              std::make_pair(1, 4));
    EXPECT_EQ(*tree.LeafMostValueMatching(Path("A/B/C"), find_one),
              std::make_pair(1, 5));
    EXPECT_EQ(*tree.LeafMostValueMatching(Path("A/B/C/D"), find_one),
              std::make_pair(1, 5));
    EXPECT_EQ(*tree.LeafMostValueMatching(Path("B"), find_one),
              std::make_pair(1, 2));
  }
  {
    Tree<IntPair> tree;
    EXPECT_EQ(tree.LeafMostValue(Path()), nullptr);
  }
}

TEST(TreeTest, ContainsMatchingValue) {
  {
    Tree<int> tree(1);
    tree.SetValueAt(Path("A"), 2);
    tree.SetValueAt(Path("A/B"), 3);
    tree.SetValueAt(Path("A/B/C"), 4);
    tree.SetValueAt(Path("A/B/D"), 5);

    EXPECT_TRUE(
        tree.ContainsMatchingValue([](int value) { return value == 1; }));
    EXPECT_TRUE(
        tree.ContainsMatchingValue([](int value) { return value == 2; }));
    EXPECT_TRUE(
        tree.ContainsMatchingValue([](int value) { return value == 3; }));
    EXPECT_TRUE(
        tree.ContainsMatchingValue([](int value) { return value == 4; }));
    EXPECT_TRUE(
        tree.ContainsMatchingValue([](int value) { return value == 5; }));
    EXPECT_FALSE(
        tree.ContainsMatchingValue([](int value) { return value == 6; }));
  }
  {
    Tree<int> tree;
    EXPECT_FALSE(
        tree.ContainsMatchingValue([](int value) { return value == 0; }));
  }
}

TEST(TreeTest, GetChild) {
  Tree<int> tree(1);
  tree.SetValueAt(Path("A"), 2);
  tree.SetValueAt(Path("B/B1"), 30);

  Tree<int>* root_string = tree.GetChild("");
  const Tree<int>* root_const_string = tree.GetChild("");
  Tree<int>* root_path = tree.GetChild(Path(""));
  const Tree<int>* root_const_path = tree.GetChild(Path(""));
  EXPECT_EQ(root_string, &tree);
  EXPECT_EQ(root_const_string, &tree);
  EXPECT_EQ(root_path, &tree);
  EXPECT_EQ(root_const_path, &tree);

  // Test A
  Tree<int>* expected_child_a = &tree.children()["A"];
  Tree<int>* child_a_string = tree.GetChild("A");
  const Tree<int>* child_a_const_string = tree.GetChild("A");
  Tree<int>* child_a_path = tree.GetChild(Path("A"));
  const Tree<int>* child_a_const_path = tree.GetChild(Path("A"));
  EXPECT_EQ(child_a_string, expected_child_a);
  EXPECT_EQ(child_a_const_string, expected_child_a);
  EXPECT_EQ(child_a_path, expected_child_a);
  EXPECT_EQ(child_a_const_path, expected_child_a);

  // Test B
  Tree<int>* expected_child_b = &tree.children()["B"];
  Tree<int>* child_b_string = tree.GetChild("B");
  const Tree<int>* child_b_const_string = tree.GetChild("B");
  Tree<int>* child_b_path = tree.GetChild(Path("B"));
  const Tree<int>* child_b_const_path = tree.GetChild(Path("B"));
  EXPECT_EQ(child_b_string, expected_child_b);
  EXPECT_EQ(child_b_const_string, expected_child_b);
  EXPECT_EQ(child_b_path, expected_child_b);
  EXPECT_EQ(child_b_const_path, expected_child_b);

  // Test B/B1
  Tree<int>* expected_child_b_b1 = &tree.children()["B"].children()["B1"];
  Tree<int>* child_b_b1_string =
      child_b_string ? child_b_string->GetChild("B1") : nullptr;
  const Tree<int>* child_b_b1_const_string =
      child_b_const_string ? child_b_const_string->GetChild("B1") : nullptr;
  Tree<int>* child_b_b1_path = tree.GetChild(Path("B/B1"));
  const Tree<int>* child_b_b1_const_path = tree.GetChild(Path("B/B1"));
  EXPECT_EQ(child_b_b1_string, expected_child_b_b1);
  EXPECT_EQ(child_b_b1_const_string, expected_child_b_b1);
  EXPECT_EQ(child_b_b1_path, expected_child_b_b1);
  EXPECT_EQ(child_b_b1_const_path, expected_child_b_b1);
  EXPECT_EQ(tree.GetChild("B/B1"), nullptr);

  // Test A/A1 (Does not exist)
  Tree<int>* child_a_a1_string =
      child_a_string ? child_a_string->GetChild("A1") : nullptr;
  const Tree<int>* child_a_a1_const_string =
      child_a_const_string ? child_a_const_string->GetChild("A1") : nullptr;
  Tree<int>* child_a_a1_path = tree.GetChild(Path("A/A1"));
  const Tree<int>* child_a_a1_const_path = tree.GetChild(Path("A/A1"));
  EXPECT_EQ(child_a_a1_string, nullptr);
  EXPECT_EQ(child_a_a1_const_string, nullptr);
  EXPECT_EQ(child_a_a1_path, nullptr);
  EXPECT_EQ(child_a_a1_const_path, nullptr);

  // Test C (Does not exist)
  Tree<int>* child_c_string = tree.GetChild("C");
  const Tree<int>* child_c_const_string = tree.GetChild("C");
  Tree<int>* child_c_path = tree.GetChild(Path("C"));
  const Tree<int>* child_c_const_path = tree.GetChild(Path("C"));
  EXPECT_EQ(child_c_string, nullptr);
  EXPECT_EQ(child_c_const_string, nullptr);
  EXPECT_EQ(child_c_path, nullptr);
  EXPECT_EQ(child_c_const_path, nullptr);
}

TEST(TreeTest, IsEmpty) {
  {
    Tree<std::string> tree;
    EXPECT_TRUE(tree.IsEmpty());
  }

  {
    Tree<int> tree(1);
    EXPECT_FALSE(tree.IsEmpty());
  }

  {
    Tree<int> tree;
    tree.SetValueAt(Path("A"), 2);
    tree.SetValueAt(Path("A/A1"), 20);
    tree.SetValueAt(Path("B/B1"), 30);
    EXPECT_FALSE(tree.IsEmpty());
    EXPECT_FALSE(tree.GetChild(Path("A"))->IsEmpty());
    EXPECT_FALSE(tree.GetChild(Path("A/A1"))->IsEmpty());
    EXPECT_FALSE(tree.GetChild(Path("B"))->IsEmpty());
    EXPECT_FALSE(tree.GetChild(Path("B/B1"))->IsEmpty());
  }
}

TEST(TreeTest, GetOrMakeSubtree) {
  Tree<int> tree;
  Tree<int>* subtree;
  tree.SetValueAt(Path("aaa/bbb/ccc"), 100);

  // Get existing subtree.
  subtree = tree.GetOrMakeSubtree(Path("aaa/bbb/ccc"));
  EXPECT_EQ(subtree->value().value(), 100);

  // Make new subtree.
  subtree = tree.GetOrMakeSubtree(Path("zzz/yyy/xxx"));
  EXPECT_NE(subtree, nullptr);
  EXPECT_FALSE(subtree->value().has_value());
  // Now set the value, and verify the pointer we're holding updated
  // appropriately.
  tree.SetValueAt(Path("zzz/yyy/xxx"), 200);
  EXPECT_TRUE(subtree->value().has_value());
  EXPECT_EQ(subtree->value().value(), 200);

  // Make new subtree along an exsiting path.
  subtree = tree.GetOrMakeSubtree(Path("aaa/bbb/mmm"));
  EXPECT_NE(subtree, nullptr);
  EXPECT_FALSE(subtree->value().has_value());
  // Now set the value, and verify the pointer we're holding updated
  // appropriately.
  tree.SetValueAt(Path("aaa/bbb/mmm"), 300);
  EXPECT_TRUE(subtree->value().has_value());
  EXPECT_EQ(subtree->value().value(), 300);
}

TEST(TreeTest, GetPath) {
  Tree<int> tree;
  const Tree<int>* subtree = tree.GetOrMakeSubtree(Path("aaa/bbb/ccc"));

  EXPECT_EQ(tree.GetPath(), Path());
  EXPECT_EQ(subtree->GetPath(), Path("aaa/bbb/ccc"));
}

// Record a list of visited node with its path and value
typedef std::vector<std::pair<std::string, int>> VisitedList;

// Get a list of visited child node with its path and value
VisitedList GetVisitedChild(const Tree<int>& tree, const Path& input_path) {
  VisitedList visited;

  tree.CallOnEach(
      input_path,
      [](const Path& path, int* value, void* data) {
        VisitedList* visited = static_cast<VisitedList*>(data);
        visited->push_back(std::make_pair(path.str(), *value));
      },
      &visited);
  return visited;
}

// Get a list of visited child node with its path and value, using std::function
VisitedList GetVisitedChildStdFunction(const Tree<int>& tree,
                                       const Path& input_path) {
  VisitedList visited;

  tree.CallOnEach(input_path, [&](const Path& path, const int& value) {
    visited.push_back(std::make_pair(path.str(), value));
  });
  return visited;
}

TEST(TreeTest, CallOnEach) {
  {
    Tree<int> tree;

    Path input_path("");
    VisitedList expected = {};

    EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
    EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
  }

  {
    Tree<int> tree(0);

    {
      Path input_path("");
      VisitedList expected = {
          {"", 0},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      Path input_path("A");
      VisitedList expected = {};

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
  }

  {
    Tree<int> tree(0);
    tree.SetValueAt(Path("A"), 1);

    {
      Path input_path("");
      VisitedList expected = {
          {"", 0},
          {"A", 1},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      Path input_path("A");
      VisitedList expected = {
          {"A", 1},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
  }

  {
    Tree<int> tree(0);
    tree.SetValueAt(Path("A"), 1);
    tree.SetValueAt(Path("A/A1"), 10);
    tree.SetValueAt(Path("A/A2/A21"), 110);
    tree.SetValueAt(Path("B/B1"), 20);

    {
      Path input_path("");
      VisitedList expected = {
          {"", 0}, {"A", 1}, {"A/A1", 10}, {"A/A2/A21", 110}, {"B/B1", 20},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      Path input_path("A");
      VisitedList expected = {
          {"A", 1},
          {"A/A1", 10},
          {"A/A2/A21", 110},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      Path input_path("A/A1");
      VisitedList expected = {
          {"A/A1", 10},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      Path input_path("A/A2");
      VisitedList expected = {
          {"A/A2/A21", 110},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      Path input_path("B");
      VisitedList expected = {
          {"B/B1", 20},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      Path input_path("B/B1");
      VisitedList expected = {
          {"B/B1", 20},
      };

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      // Does not exist
      Path input_path("B/B2");
      VisitedList expected = {};

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
    {
      // Does not exist
      Path input_path("B/B1/B11");
      VisitedList expected = {};

      EXPECT_EQ(GetVisitedChild(tree, input_path), expected);
      EXPECT_EQ(GetVisitedChildStdFunction(tree, input_path), expected);
    }
  }
}

TEST(TreeTest, CallOnEachAncestorIncludeSelf) {
  std::vector<int> call_order;
  std::vector<int> expected_call_order{3, 2, 1};

  Tree<int> tree;
  tree.set_value(1);
  tree.SetValueAt(Path("aaa"), 2);
  tree.SetValueAt(Path("aaa/bbb"), 3);
  tree.SetValueAt(Path("aaa/bbb/ccc"), 4);
  Tree<int>* subtree = tree.GetChild(Path("aaa/bbb"));

  subtree->CallOnEachAncestor(
      [&call_order](Tree<int>* current_tree) {
        Optional<int> value = current_tree->value();
        call_order.push_back(*value);
        return false;
      },
      true);

  EXPECT_EQ(call_order, expected_call_order);
}

TEST(TreeTest, CallOnEachAncestorDoNotIncludeSelf) {
  std::vector<int> call_order;
  std::vector<int> expected_call_order{2, 1};

  Tree<int> tree;
  tree.set_value(1);
  tree.SetValueAt(Path("aaa"), 2);
  tree.SetValueAt(Path("aaa/bbb"), 3);
  tree.SetValueAt(Path("aaa/bbb/ccc"), 4);
  Tree<int>* subtree = tree.GetChild(Path("aaa/bbb"));

  subtree->CallOnEachAncestor(
      [&call_order](Tree<int>* current_tree) {
        Optional<int> value = current_tree->value();
        call_order.push_back(*value);
        return false;
      },
      false);

  EXPECT_EQ(call_order, expected_call_order);
}

TEST(TreeTest, CallOnEachDescendantIncludeSelf) {
  std::vector<int> call_order;
  std::vector<int> expected_call_order{4};

  Tree<int> tree;
  tree.set_value(1);
  tree.SetValueAt(Path("aaa"), 2);
  tree.SetValueAt(Path("aaa/bbb"), 3);
  tree.SetValueAt(Path("aaa/bbb/ccc"), 4);
  Tree<int>* subtree = tree.GetChild(Path("aaa/bbb"));

  subtree->CallOnEachDescendant([&call_order](Tree<int>* current_tree) {
    Optional<int> value = current_tree->value();
    call_order.push_back(*value);
    return false;
  });

  EXPECT_EQ(call_order, expected_call_order);
}

TEST(TreeTest, CallOnEachDescendantDoNotIncludeSelf) {
  std::vector<int> call_order;
  std::vector<int> expected_call_order{3, 4};

  Tree<int> tree;
  tree.set_value(1);
  tree.SetValueAt(Path("aaa"), 2);
  tree.SetValueAt(Path("aaa/bbb"), 3);
  tree.SetValueAt(Path("aaa/bbb/ccc"), 4);
  Tree<int>* subtree = tree.GetChild(Path("aaa/bbb"));

  subtree->CallOnEachDescendant(
      [&call_order](Tree<int>* current_tree) {
        Optional<int> value = current_tree->value();
        call_order.push_back(*value);
        return false;
      },
      true);

  EXPECT_EQ(call_order, expected_call_order);
}

TEST(TreeTest, CallOnEachDescendantChildrenFirst) {
  std::vector<int> call_order;
  std::vector<int> expected_call_order{4, 3};

  Tree<int> tree;
  tree.set_value(1);
  tree.SetValueAt(Path("aaa"), 2);
  tree.SetValueAt(Path("aaa/bbb"), 3);
  tree.SetValueAt(Path("aaa/bbb/ccc"), 4);
  Tree<int>* subtree = tree.GetChild(Path("aaa/bbb"));

  subtree->CallOnEachDescendant(
      [&call_order](Tree<int>* current_tree) {
        Optional<int> value = current_tree->value();
        call_order.push_back(*value);
        return false;
      },
      true, true);

  EXPECT_EQ(call_order, expected_call_order);
}

TEST(TreeTest, CallOnEachDescendantChildrenLast) {
  std::vector<int> call_order;
  std::vector<int> expected_call_order{3, 4};

  Tree<int> tree;
  tree.set_value(1);
  tree.SetValueAt(Path("aaa"), 2);
  tree.SetValueAt(Path("aaa/bbb"), 3);
  tree.SetValueAt(Path("aaa/bbb/ccc"), 4);
  Tree<int>* subtree = tree.GetChild(Path("aaa/bbb"));

  subtree->CallOnEachDescendant(
      [&call_order](Tree<int>* current_tree) {
        Optional<int> value = current_tree->value();
        call_order.push_back(*value);
        return false;
      },
      true, false);

  EXPECT_EQ(call_order, expected_call_order);
}

TEST(TreeTest, FindRootMostPathWithValueSuccess) {
  Tree<int> tree;
  tree.SetValueAt(Path("1/2/3"), 100);
  tree.SetValueAt(Path("1/2/3/4/5/6"), 200);

  Optional<Path> result = tree.FindRootMostPathWithValue(Path("1/2/3/4/5/6/7"));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), Path("1/2/3"));
}

TEST(TreeTest, FindRootMostPathWithValueNoValue) {
  Tree<int> tree;
  tree.SetValueAt(Path("a/b/c"), 100);
  tree.SetValueAt(Path("a/b/c/d/e/f"), 200);

  Optional<Path> result = tree.FindRootMostPathWithValue(Path("1/2/3/4/5/6/7"));
  EXPECT_FALSE(result.has_value());
}

TEST(TreeTest, FindRootMostMatchingPathSuccess) {
  Tree<int> tree;
  tree.SetValueAt(Path("1"), 1);
  tree.SetValueAt(Path("1/2"), 3);
  tree.SetValueAt(Path("1/2/3"), 6);
  tree.SetValueAt(Path("1/2/3/4"), 10);
  tree.SetValueAt(Path("1/2/3/4/5"), 15);
  tree.SetValueAt(Path("1/2/3/4/5/6"), 21);

  Optional<Path> result = tree.FindRootMostMatchingPath(
      Path("1/2/3/4/5/6"), [](int value) { return value == 10; });
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), Path("1/2/3/4"));
}

TEST(TreeTest, FindRootMostMatchingPathNoMatch) {
  Tree<int> tree;
  tree.SetValueAt(Path("1"), 1);
  tree.SetValueAt(Path("1/2"), 3);
  tree.SetValueAt(Path("1/2/3"), 6);
  tree.SetValueAt(Path("1/2/3/4"), 10);
  tree.SetValueAt(Path("1/2/3/4/5"), 15);
  tree.SetValueAt(Path("1/2/3/4/5/6"), 21);

  Optional<Path> result = tree.FindRootMostMatchingPath(
      Path("1/2/3/4/5/6"), [](int value) { return value == 100; });
  EXPECT_FALSE(result.has_value());
}

TEST(TreeTest, Fold) {
  Tree<char> tree;
  tree.SetValueAt(Path("1/1"), 'H');
  tree.SetValueAt(Path("1/2"), 'e');
  tree.SetValueAt(Path("1/3"), 'l');
  tree.SetValueAt(Path("1/4/1"), 'l');
  tree.SetValueAt(Path("1/4"), 'o');
  tree.SetValueAt(Path("1"), ',');
  tree.SetValueAt(Path("2"), ' ');
  tree.SetValueAt(Path("3/1/1"), 'w');
  tree.SetValueAt(Path("3/1/2"), 'o');
  tree.SetValueAt(Path("3/1"), 'r');
  tree.SetValueAt(Path("3/2"), 'l');
  tree.SetValueAt(Path("3"), 'd');
  tree.SetValueAt(Path("4"), '!');

  std::string result = tree.Fold(
      std::string(),
      [](Path path, char value, std::string accum) { return accum += value; });

  EXPECT_EQ(result, "Hello, world!");
}

TEST(TreeTest, Equality) {
  Tree<char> tree;
  tree.SetValueAt(Path("1/1"), 'H');
  tree.SetValueAt(Path("1/2"), 'e');
  tree.SetValueAt(Path("1/3"), 'l');
  tree.SetValueAt(Path("1/4/1"), 'l');
  tree.SetValueAt(Path("1/4"), 'o');
  tree.SetValueAt(Path("1"), ',');
  tree.SetValueAt(Path("2"), ' ');
  tree.SetValueAt(Path("3/1/1"), 'w');
  tree.SetValueAt(Path("3/1/2"), 'o');
  tree.SetValueAt(Path("3/1"), 'r');
  tree.SetValueAt(Path("3/2"), 'l');
  tree.SetValueAt(Path("3"), 'd');
  tree.SetValueAt(Path("4"), '!');

  Tree<char> same_tree;
  same_tree.SetValueAt(Path("1/1"), 'H');
  same_tree.SetValueAt(Path("1/2"), 'e');
  same_tree.SetValueAt(Path("1/3"), 'l');
  same_tree.SetValueAt(Path("1/4/1"), 'l');
  same_tree.SetValueAt(Path("1/4"), 'o');
  same_tree.SetValueAt(Path("1"), ',');
  same_tree.SetValueAt(Path("2"), ' ');
  same_tree.SetValueAt(Path("3/1/1"), 'w');
  same_tree.SetValueAt(Path("3/1/2"), 'o');
  same_tree.SetValueAt(Path("3/1"), 'r');
  same_tree.SetValueAt(Path("3/2"), 'l');
  same_tree.SetValueAt(Path("3"), 'd');
  same_tree.SetValueAt(Path("4"), '!');

  Tree<char> different_tree;
  different_tree.SetValueAt(Path("1/1"), 'H');
  different_tree.SetValueAt(Path("1/2"), 'E');
  different_tree.SetValueAt(Path("1/3"), 'L');
  different_tree.SetValueAt(Path("1/4/1"), 'L');
  different_tree.SetValueAt(Path("1/4"), 'O');
  different_tree.SetValueAt(Path("1"), '!');
  different_tree.SetValueAt(Path("2"), ' ');
  different_tree.SetValueAt(Path("3/1/1"), 'w');
  different_tree.SetValueAt(Path("3/1/2"), 'a');
  different_tree.SetValueAt(Path("3/1"), 'r');
  different_tree.SetValueAt(Path("3/2"), 'l');
  different_tree.SetValueAt(Path("3"), 'd');
  different_tree.SetValueAt(Path("4"), '?');

  EXPECT_EQ(tree, same_tree);
  EXPECT_NE(tree, different_tree);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
