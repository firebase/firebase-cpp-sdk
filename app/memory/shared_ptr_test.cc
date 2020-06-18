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

#include "app/memory/shared_ptr.h"

#include <thread>  // NOLINT
#include <vector>

#include "app/memory/atomic.h"
#include "app/meta/move.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace {

using ::firebase::compat::Atomic;
using ::testing::Eq;
using ::testing::IsNull;

class Destructable {
 public:
  explicit Destructable(Atomic<uint64_t>* destroyed) : destroyed_(destroyed) {}
  virtual ~Destructable() { destroyed_->fetch_add(1); }

 private:
  Atomic<uint64_t>* const destroyed_;
};

class Derived : public Destructable {
 public:
  explicit Derived(Atomic<uint64_t>* destroyed) : Destructable(destroyed) {}
};

TEST(SharedPtrTest, DefaultConstructedSharedPtrDoesNotManageAnObject) {
  SharedPtr<Destructable> ptr;
  EXPECT_THAT(ptr.use_count(), Eq(0));
  EXPECT_THAT(ptr.get(), Eq(nullptr));
}

TEST(SharedPtrTest, EmptySharedPtrCopiesDoNotManageAnObject) {
  SharedPtr<Destructable> ptr;
  SharedPtr<Destructable> ptr2(ptr);  // NOLINT
  EXPECT_THAT(ptr.use_count(), Eq(0));
  EXPECT_THAT(ptr2.use_count(), Eq(0));
  EXPECT_THAT(ptr.get(), Eq(nullptr));
  EXPECT_THAT(ptr2.get(), Eq(nullptr));
}

TEST(SharedPtrTest, NullptrConstructedSharedPtrDoesNotManageAnObject) {
  SharedPtr<Destructable> ptr(static_cast<Destructable*>(nullptr));
  EXPECT_THAT(ptr.use_count(), Eq(0));
  EXPECT_THAT(ptr.get(), Eq(nullptr));
}

TEST(SharedPtrTest, WrapSharedCreatesValidSharedPtr) {
  Atomic<uint64_t> destroyed;
  {
    auto destructable = new Destructable(&destroyed);
    auto ptr = WrapShared(destructable);
  }
  EXPECT_THAT(destroyed.load(), Eq(1));
}

TEST(SharedPtrTest, SharedPtrCorrectlyDestroysTheContainedObject) {
  Atomic<uint64_t> destroyed;
  {
    auto ptr = MakeShared<Destructable>(&destroyed);
    EXPECT_THAT(ptr.use_count(), Eq(1));
  }
  EXPECT_THAT(destroyed.load(), Eq(1));
}

TEST(SharedPtrTest, CopiesShareTheSameObjectWhichIsDestroyedOnlyOnce) {
  Atomic<uint64_t> destroyed;
  {
    auto ptr = MakeShared<Destructable>(&destroyed);
    EXPECT_THAT(ptr.use_count(), Eq(1));
    {
      auto ptr2 = ptr;  // NOLINT
      EXPECT_THAT(ptr.use_count(), Eq(2));
      EXPECT_THAT(ptr.get(), Eq(ptr2.get()));
    }
    EXPECT_THAT(ptr.use_count(), Eq(1));
    EXPECT_THAT(destroyed.load(), Eq(0));
  }
  EXPECT_THAT(destroyed.load(), Eq(1));
}

TEST(SharedPtrTest, MoveCorrectlyTransfersOwnership) {
  Atomic<uint64_t> destroyed;
  {
    auto ptr = MakeShared<Destructable>(&destroyed);
    EXPECT_THAT(ptr.use_count(), Eq(1));
    {
      auto* managed = ptr.get();
      auto ptr2 = Move(ptr);
      EXPECT_THAT(ptr.use_count(), Eq(0));
      EXPECT_THAT(ptr.get(), Eq(nullptr));
      EXPECT_THAT(ptr2.use_count(), Eq(1));
      EXPECT_THAT(ptr2.get(), Eq(managed));
    }
    EXPECT_THAT(destroyed.load(), Eq(1));
  }
  EXPECT_THAT(destroyed.load(), Eq(1));
}

TEST(SharedPtrTest,
     ConvertingCopiesShareTheSameObjectWhichIsDestroyedOnlyOnce) {
  Atomic<uint64_t> destroyed;
  {
    auto ptr = MakeShared<Derived>(&destroyed);
    EXPECT_THAT(ptr.use_count(), Eq(1));
    {
      SharedPtr<Destructable> ptr2(ptr);  // NOLINT
      EXPECT_THAT(ptr.use_count(), Eq(2));
      EXPECT_THAT(ptr.get(), Eq(ptr2.get()));
    }
    EXPECT_THAT(ptr.use_count(), Eq(1));
    EXPECT_THAT(destroyed.load(), Eq(0));
  }
  EXPECT_THAT(destroyed.load(), Eq(1));
}

TEST(SharedPtrTest, ConvertingMoveCorrectlyTransfersOwnership) {
  Atomic<uint64_t> destroyed;
  {
    auto ptr = MakeShared<Derived>(&destroyed);
    EXPECT_THAT(ptr.use_count(), Eq(1));
    {
      auto* managed = ptr.get();
      SharedPtr<Destructable> ptr2(Move(ptr));
      EXPECT_THAT(ptr.use_count(), Eq(0));
      EXPECT_THAT(ptr.get(), Eq(nullptr));
      EXPECT_THAT(ptr2.use_count(), Eq(1));
      EXPECT_THAT(ptr2.get(), Eq(managed));
    }
    EXPECT_THAT(destroyed.load(), Eq(1));
  }
  EXPECT_THAT(destroyed.load(), Eq(1));
}

TEST(SharedPtrTest, EmptySharedPtrIsFalseWhenConvertedToBool) {
  SharedPtr<int> ptr;
  EXPECT_FALSE(ptr);
}

TEST(SharedPtrTest, NontEmptySharedPtrIsTrueWhenConvertedToBool) {
  auto ptr = MakeShared<int>(1);
  EXPECT_TRUE(ptr);
}

TEST(SharedPtrTest,
     SharedPtrRefCountIsThreadSafeAndOnlyDeletesTheManagedPtrOnce) {
  Atomic<uint64_t> destroyed;
  std::vector<std::thread> threads;
  {
    auto ptr = MakeShared<Destructable>(&destroyed);

    for (int i = 0; i < 10; ++i) {
      threads.emplace_back([ptr] {
        // make another copy.
        auto ptr2 = ptr;  // NOLINT
      });
    }
    EXPECT_THAT(destroyed.load(), Eq(0));
  }
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_THAT(destroyed.load(), Eq(1));
}

TEST(SharedPtrTest, CopySharedPtr) {
  SharedPtr<int> *value1 = new SharedPtr<int>(new int(10));
  SharedPtr<int> *value2 = new SharedPtr<int>();
  *value2 = *value1;
  delete value1;
  EXPECT_THAT(**value2, 10);
  delete value2;
}

TEST(SharedPtrTest, CopySharedPtrDereferenceTest) {
  SharedPtr<int> ptr1 = MakeShared<int>(10);
  SharedPtr<int> ptr2 = MakeShared<int>(10);
  SharedPtr<int> ptr3 = MakeShared<int>(10);
  SharedPtr<int> ptr = ptr1;
  ptr = ptr2;
  EXPECT_THAT(ptr1.use_count(), Eq(1));
  ptr = ptr3;
  EXPECT_THAT(ptr2.use_count(), Eq(1));
}

TEST(SharedPtrTest, SharedPtrReset) {
  SharedPtr<int> ptr1 = MakeShared<int>(10);
  ptr1.reset();
  EXPECT_THAT(ptr1.get(), IsNull());  // NOLINT

  SharedPtr<int> ptr2 = MakeShared<int>(10);
  SharedPtr<int> ptr3 = ptr2;
  ptr3.reset();
  EXPECT_THAT(ptr3.get(), IsNull());  // NOLINT
  EXPECT_THAT(ptr2.use_count(), Eq(1));
}

TEST(SharedPtrTest, MoveSharedPtr) {
  SharedPtr<int> value1(new int(10));
  SharedPtr<int> value2;
  EXPECT_THAT(*value1, Eq(10));
  value2 = Move(value1);
  EXPECT_THAT(value1.get(), IsNull());  // NOLINT
  EXPECT_THAT(*value2, Eq(10));
}

}  // namespace
}  // namespace firebase
