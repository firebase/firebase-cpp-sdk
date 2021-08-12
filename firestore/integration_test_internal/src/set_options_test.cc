// Copyright 2021 Google LLC

#include <stdint.h>

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
