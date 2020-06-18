/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/memory/unique_ptr.h"

#include "app/meta/move.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace {

using ::testing::Eq;

typedef void (*OnDestroyFn)(bool*);

class Destructable {
 public:
  explicit Destructable(bool* destroyed) : destroyed_(destroyed) {}
  ~Destructable() { *destroyed_ = true; }

  bool destroyed() const { return destroyed_; }

 private:
  bool* const destroyed_;
};

class Base {
 public:
  virtual ~Base() {}
};

class Derived : public Base {
 public:
  Derived(bool* destroyed) : destroyed_(destroyed) {}
  ~Derived() override { *destroyed_ = true; }
  bool destroyed() const { return destroyed_; }

 private:
  bool* const destroyed_;
};

void Foo(UniquePtr<Base> b) {}

void AssertRawPtrEq(const UniquePtr<Destructable>& ptr, Destructable* value) {
  ASSERT_THAT(ptr.get(), Eq(value));
  ASSERT_THAT(ptr.operator->(), Eq(value));

  if (value != nullptr) {
    ASSERT_THAT((*ptr).destroyed(), Eq(value->destroyed()));
    ASSERT_THAT(ptr->destroyed(), Eq(value->destroyed()));
  }
}

TEST(UniquePtrTest, DeletesContainingPtrWhenDestroyed) {
  bool destroyed = false;
  { MakeUnique<Destructable>(&destroyed); }
  EXPECT_THAT(destroyed, Eq(true));
}

TEST(UniquePtrTest, DoesNotDeleteContainingPtrWhenDestroyedIfReleased) {
  bool destroyed = false;
  Destructable* raw_ptr;
  {
    auto ptr = MakeUnique<Destructable>(&destroyed);
    raw_ptr = ptr.release();
  }
  EXPECT_THAT(destroyed, Eq(false));
  delete raw_ptr;
  EXPECT_THAT(destroyed, Eq(true));
}

TEST(UniquePtrTest, MoveConstructionTransfersOwnershipOfTheUnderlyingPtr) {
  bool destroyed = false;
  {
    auto ptr = MakeUnique<Destructable>(&destroyed);
    auto* raw_ptr = ptr.get();
    auto movedInto = UniquePtr<Destructable>(Move(ptr));

    AssertRawPtrEq(ptr, nullptr);
    AssertRawPtrEq(movedInto, raw_ptr);
  }
}

TEST(UniquePtrTest, CopyConstructionTransfersOwnershipOfTheUnderlyingPtr) {
  bool destroyed = false;
  {
    auto ptr = MakeUnique<Destructable>(&destroyed);
    auto* raw_ptr = ptr.get();
    auto movedInto = UniquePtr<Destructable>(ptr);

    AssertRawPtrEq(ptr, nullptr);
    AssertRawPtrEq(movedInto, raw_ptr);
  }
}

TEST(UniquePtrTest, MoveAssignmentTransfersOwnershipOfTheUnderlyingPtr) {
  bool destroyed1 = false;
  bool destroyed2 = false;
  {
    auto ptr1 = MakeUnique<Destructable>(&destroyed1);
    auto ptr2 = MakeUnique<Destructable>(&destroyed2);

    auto* raw_ptr2 = ptr2.get();
    ptr1 = Move(ptr2);

    ASSERT_THAT(destroyed1, Eq(true));
    AssertRawPtrEq(ptr1, raw_ptr2);
    AssertRawPtrEq(ptr2, nullptr);
  }
  ASSERT_THAT(destroyed2, Eq(true));
}

TEST(UniquePtrTest, CopyAssignmentTransfersOwnershipOfTheUnderlyingPtr) {
  bool destroyed1 = false;
  bool destroyed2 = false;
  {
    auto ptr1 = MakeUnique<Destructable>(&destroyed1);
    auto ptr2 = MakeUnique<Destructable>(&destroyed2);

    auto* raw_ptr2 = ptr2.get();
    ptr1 = ptr2;

    ASSERT_THAT(destroyed1, Eq(true));
    AssertRawPtrEq(ptr1, raw_ptr2);
    AssertRawPtrEq(ptr2, nullptr);
  }
  ASSERT_THAT(destroyed2, Eq(true));
}

TEST(UniquePtrTest, MoveAssignmentToEmptyTransfersOwnershipOfThePtr) {
  bool destroyed = false;
  {
    UniquePtr<Destructable> ptr;
    AssertRawPtrEq(ptr, nullptr);

    auto raw_ptr = new Destructable(&destroyed);
    ptr = raw_ptr;
    AssertRawPtrEq(ptr, raw_ptr);
  }
  ASSERT_THAT(destroyed, Eq(true));
}

TEST(UniquePtrTest, EmptyUniquePtrImplicitlyConvertsToFalse) {
  UniquePtr<int> ptr;
  EXPECT_THAT(ptr, Eq(false));
}

TEST(UniquePtrTest, NonEmptyUniquePtrImplicitlyConvertsToTrue) {
  auto ptr = MakeUnique<int>(10);
  EXPECT_THAT(ptr, Eq(true));
}

TEST(UniquePtrTest, UniquePtrToDerivedConvertsToBase) {
  bool destroyed = false;
  { UniquePtr<Base> base_ptr = MakeUnique<Derived>(&destroyed); }
  EXPECT_THAT(destroyed, Eq(true));
}

}  // namespace
}  // namespace firebase
