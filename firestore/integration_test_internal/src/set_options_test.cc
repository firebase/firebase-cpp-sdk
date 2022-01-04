/*
 * Copyright 2021 Google LLC
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

#include "firebase/firestore.h"

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

TEST(SetOptionsTest, Equality) {
  SetOptions opt1 = SetOptions();
  SetOptions opt2 = SetOptions();
  SetOptions opt3 = SetOptions::Merge();
  SetOptions opt4 = SetOptions::Merge();
  SetOptions opt5 = SetOptions::MergeFields({"a", "a", "b", "c.d"});
  SetOptions opt6 = SetOptions::MergeFields({"c.d", "b", "a", "b"});
  SetOptions opt7 = SetOptions::MergeFields({"b"});

  EXPECT_TRUE(opt1 == opt1);
  EXPECT_TRUE(opt1 == opt2);
  EXPECT_TRUE(opt1 != opt3);
  EXPECT_TRUE(opt1 != opt4);
  EXPECT_TRUE(opt1 != opt5);
  EXPECT_TRUE(opt1 != opt6);
  EXPECT_TRUE(opt1 != opt7);
  EXPECT_TRUE(opt3 == opt4);
  EXPECT_TRUE(opt3 != opt5);
  EXPECT_TRUE(opt3 != opt6);
  EXPECT_TRUE(opt3 != opt7);
  EXPECT_TRUE(opt5 == opt6);
  EXPECT_TRUE(opt5 != opt7);

  EXPECT_FALSE(opt1 != opt1);
  EXPECT_FALSE(opt1 != opt2);
  EXPECT_FALSE(opt1 == opt3);
  EXPECT_FALSE(opt1 == opt4);
  EXPECT_FALSE(opt1 == opt5);
  EXPECT_FALSE(opt1 == opt6);
  EXPECT_FALSE(opt1 == opt7);
  EXPECT_FALSE(opt3 != opt4);
  EXPECT_FALSE(opt3 == opt5);
  EXPECT_FALSE(opt3 == opt6);
  EXPECT_FALSE(opt3 == opt7);
  EXPECT_FALSE(opt5 != opt6);
  EXPECT_FALSE(opt5 == opt7);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
