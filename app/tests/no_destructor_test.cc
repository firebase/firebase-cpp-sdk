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

#include <map>
#include <string>

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

TEST(NoDestructorTest, ConstAccess) {
  const NoDestructor<std::string> str("const");
  EXPECT_EQ(*str, "const");
  EXPECT_EQ(str->size(), 5u);
  EXPECT_EQ(str.get(), &*str);
}

}  // namespace
}  // namespace firebase
