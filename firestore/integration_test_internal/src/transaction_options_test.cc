/*
 * Copyright 2022 Google LLC
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

TEST(TransactionOptionsTest, DefaultConstructor) {
  TransactionOptions options;
  EXPECT_EQ(options.max_attempts(), 5);
}

TEST(TransactionOptionsTest, SetMaxAttemptsSetsValidValues) {
  TransactionOptions options;
  options.set_max_attempts(10);
  EXPECT_EQ(options.max_attempts(), 10);
  options.set_max_attempts(1);
  EXPECT_EQ(options.max_attempts(), 1);
  options.set_max_attempts(2);
  EXPECT_EQ(options.max_attempts(), 2);
  options.set_max_attempts(INT32_MAX);
  EXPECT_EQ(options.max_attempts(), INT32_MAX);
}

TEST(TransactionOptionsTest, SetMaxAttemptsThrowsOnInvalidValues) {
  TransactionOptions options;
  EXPECT_ANY_THROW(options.set_max_attempts(0));
  EXPECT_ANY_THROW(options.set_max_attempts(-1));
  EXPECT_ANY_THROW(options.set_max_attempts(INT32_MIN));
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
