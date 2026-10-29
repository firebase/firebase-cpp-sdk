/*
 * Copyright 2026 Google LLC
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

#include "app/src/no_destructor.h"

#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "app/src/include/firebase/variant.h"
#include "gtest/gtest.h"

namespace firebase {
namespace {

// Records how many times it has been constructed and destroyed.
struct Tracked {
  explicit Tracked(int value_in) : value(value_in) { ++constructed; }
  ~Tracked() { ++destroyed; }

  int value;
  static int constructed;
  static int destroyed;
};
int Tracked::constructed = 0;
int Tracked::destroyed = 0;

struct alignas(64) OverAligned {
  explicit OverAligned(uint64_t seed_in) : seed(seed_in) { ++constructed; }
  ~OverAligned() { ++destroyed; }

  uint64_t seed;
  static int constructed;
  static int destroyed;
};
int OverAligned::constructed = 0;
int OverAligned::destroyed = 0;

// NoDestructor<T> must be trivially destructible so function-local statics do
// not register __cxa_atexit / atexit handlers, and must remain non-copyable and
// non-movable even when T has broad converting constructors (such as Variant).
static_assert(std::is_trivially_destructible<NoDestructor<Tracked>>::value,
              "NoDestructor<T> must be trivially destructible.");
static_assert(!std::is_copy_constructible<NoDestructor<Tracked>>::value,
              "NoDestructor<T> must not be copy constructible.");
static_assert(!std::is_move_constructible<NoDestructor<Tracked>>::value,
              "NoDestructor<T> must not be move constructible.");
static_assert(!std::is_copy_assignable<NoDestructor<Tracked>>::value,
              "NoDestructor<T> must not be copy assignable.");
static_assert(!std::is_move_assignable<NoDestructor<Tracked>>::value,
              "NoDestructor<T> must not be move assignable.");
static_assert(
    !std::is_constructible<NoDestructor<Variant>,
                           NoDestructor<Variant>&>::value,
    "Non-const lvalue NoDestructor<Variant> must not hijack forwarding ctor.");
static_assert(
    !std::is_constructible<NoDestructor<Variant>,
                           const NoDestructor<Variant>&>::value,
    "Const lvalue NoDestructor<Variant> must not be copy constructible.");
static_assert(!std::is_constructible<NoDestructor<Variant>,
                                     NoDestructor<Variant>&&>::value,
              "Rvalue NoDestructor<Variant> must not be move constructible.");

TEST(NoDestructorTest, ForwardsConstructorArguments) {
  NoDestructor<std::string> str("hello");
  EXPECT_EQ(*str, "hello");
  EXPECT_EQ(str->size(), 5u);
  EXPECT_EQ(str.get(), &*str);
}

TEST(NoDestructorTest, DefaultConstructs) {
  NoDestructor<std::map<int, int>> map;
  EXPECT_TRUE(map->empty());
  (*map)[1] = 2;
  EXPECT_EQ(map->at(1), 2);
}

TEST(NoDestructorTest, BraceEnclosedInitializerList) {
  NoDestructor<std::vector<int>> values({10, 20, 30});
  ASSERT_EQ(values->size(), 3u);
  EXPECT_EQ((*values)[0], 10);
  EXPECT_EQ((*values)[1], 20);
  EXPECT_EQ((*values)[2], 30);
}

TEST(NoDestructorTest, DoesNotRunDestructorWhenGoingOutOfScope) {
  Tracked::constructed = 0;
  Tracked::destroyed = 0;
  {
    NoDestructor<Tracked> tracked(42);
    EXPECT_EQ(tracked->value, 42);
    EXPECT_EQ(Tracked::constructed, 1);
  }
  EXPECT_EQ(Tracked::constructed, 1);
  EXPECT_EQ(Tracked::destroyed, 0);
}

TEST(NoDestructorTest, PreservesOverAlignedStorage) {
  OverAligned::constructed = 0;
  OverAligned::destroyed = 0;
  {
    NoDestructor<OverAligned> aligned(0x12345678ULL);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned.get()) % alignof(OverAligned),
              0u);
    EXPECT_EQ(aligned->seed, 0x12345678ULL);
    const NoDestructor<OverAligned>& const_aligned = aligned;
    EXPECT_EQ(const_aligned->seed, 0x12345678ULL);
  }
  EXPECT_EQ(OverAligned::constructed, 1);
  EXPECT_EQ(OverAligned::destroyed, 0);
}

TEST(NoDestructorTest, ConstAccess) {
  const NoDestructor<std::string> str("const");
  EXPECT_EQ(*str, "const");
  EXPECT_EQ(str->size(), 5u);
  EXPECT_EQ(str.get(), &*str);
}

}  // namespace
}  // namespace firebase
