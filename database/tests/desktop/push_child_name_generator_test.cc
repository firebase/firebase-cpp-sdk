// Copyright 2017 Google LLC
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

#include "database/src/desktop/push_child_name_generator.h"

#include <set>
#include <vector>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "thread/fiber/fiber.h"

namespace {

using ::firebase::database::internal::PushChildNameGenerator;
using ::testing::Eq;
using ::testing::Lt;

TEST(PushChildNameGeneratorTest, TestOrderOfGeneratedNamesSameTime) {
  PushChildNameGenerator generator;

  // Names should be generated in a way such that they are lexicographically
  // increasing.
  std::vector<std::string> keys;
  keys.reserve(100);
  for (int i = 0; i < 100; ++i) {
    keys.push_back(generator.GeneratePushChildName(0));
  }
  for (int i = 0; i < 99; ++i) {
    EXPECT_THAT(keys[i], Lt(keys[i + 1]));
  }
}

TEST(PushChildNameGeneratorTest, TestOrderOfGeneratedNamesDifferentTime) {
  PushChildNameGenerator generator;
  const int kNumToTest = 100;

  // Names should be generated in a way such that they are lexicographically
  // increasing.
  std::vector<std::string> keys;
  keys.reserve(kNumToTest);
  for (int i = 0; i < kNumToTest; ++i) {
    keys.push_back(generator.GeneratePushChildName(i));
  }
  for (int i = 0; i < kNumToTest - 1; ++i) {
    EXPECT_THAT(keys[i], Lt(keys[i + 1]));
  }
}

TEST(PushChildNameGeneratorTest, TestSimultaneousGeneratedNames) {
  PushChildNameGenerator generator;
  const int kNumToTest = 100;

  // Create a bunch of keys.
  std::vector<std::string> keys;
  keys.resize(kNumToTest);
  std::vector<thread::Fiber*> fibers;
  for (int i = 0; i < kNumToTest; i++) {
    fibers.push_back(new thread::Fiber([&generator, &keys, i]() {
      keys[i] = generator.GeneratePushChildName(std::time(nullptr));
    }));
  }

  // Insert keys into set. If there is a duplicate key, it will be discarded.
  std::set<std::string> key_set;
  for (int i = 0; i < kNumToTest; i++) {
    fibers[i]->Join();
    key_set.insert(keys[i]);
    delete fibers[i];
    fibers[i] = nullptr;
  }

  // Ensure that all keys are unique by making sure no keys were discarded.
  EXPECT_THAT(key_set.size(), Eq(kNumToTest));
}

}  // namespace
