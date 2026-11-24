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

#include <string>
#include <type_traits>

#include "gtest/gtest.h"

namespace firebase {
namespace internal {
namespace {

struct CountingType {
  int value;
  int* destroyed_count;
  CountingType(int v, int* count) : value(v), destroyed_count(count) {}
  ~CountingType() { ++(*destroyed_count); }
};

TEST(NoDestructorTest, TriviallyDestructible) {
  static_assert(std::is_trivially_destructible_v<NoDestructor<std::string>>);
  static_assert(std::is_trivially_destructible_v<NoDestructor<CountingType>>);
  SUCCEED();
}

TEST(NoDestructorTest, ConstructsInPlaceAndNeverDestroys) {
  int destroyed = 0;
  {
    NoDestructor<CountingType> holder(42, &destroyed);
    EXPECT_EQ(holder->value, 42);
    EXPECT_EQ((*holder).value, 42);
    EXPECT_EQ(holder.get()->value, 42);
  }
  EXPECT_EQ(destroyed, 0);
}

TEST(NoDestructorTest, SupportsStandardContainers) {
  NoDestructor<std::string> str("hello");
  EXPECT_EQ(*str, "hello");
  str->append(" world");
  EXPECT_EQ(*str, "hello world");
}

}  // namespace
}  // namespace internal
}  // namespace firebase
