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

#include <sstream>
#include <type_traits>
#include <utility>

#include "firebase/firestore.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

TEST(TransactionOptionsTest, TypeTraits) {
  static_assert(std::is_trivially_copyable<TransactionOptions>::value,
                "Update the public doxygen comments about TransactionOptions "
                "being trivially copyable in the header file if it ever "
                "changes to NOT be trivially copyable.");
}

TEST(TransactionOptionsTest, DefaultConstructor) {
  TransactionOptions options;
  EXPECT_EQ(options.max_attempts(), 5);
}

TEST(TransactionOptionsTest, CopyConstructor) {
  TransactionOptions options;
  options.set_max_attempts(99);

  TransactionOptions copied_options(options);

  EXPECT_EQ(options.max_attempts(), 99);
  EXPECT_EQ(copied_options.max_attempts(), 99);
}

TEST(TransactionOptionsTest, CopyAssignmentOperator) {
  TransactionOptions options;
  options.set_max_attempts(99);
  TransactionOptions options_copy_dest;
  options_copy_dest.set_max_attempts(333);

  options_copy_dest = options;

  EXPECT_EQ(options.max_attempts(), 99);
  EXPECT_EQ(options_copy_dest.max_attempts(), 99);
}

TEST(TransactionOptionsTest, MoveConstructor) {
  TransactionOptions options;
  options.set_max_attempts(99);

  TransactionOptions moved_options(std::move(options));

  EXPECT_EQ(moved_options.max_attempts(), 99);
}

TEST(TransactionOptionsTest, MoveAssignmentOperator) {
  TransactionOptions options;
  options.set_max_attempts(99);
  TransactionOptions options_move_dest;
  options_move_dest.set_max_attempts(333);

  options_move_dest = std::move(options);

  EXPECT_EQ(options_move_dest.max_attempts(), 99);
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

TEST(TransactionOptionsTest, ToString) {
  TransactionOptions options;
  options.set_max_attempts(42);

  const std::string to_string_result = options.ToString();

  EXPECT_EQ(to_string_result, "TransactionOptions(max_attempts=42)");
}

TEST(TransactionOptionsTest, RightShiftOperatorToOutputStream) {
  TransactionOptions options;
  options.set_max_attempts(42);
  const std::string expected_str = options.ToString();
  std::ostringstream ss;

  ss << options;

  EXPECT_EQ(ss.str(), expected_str);
}

TEST(TransactionOptionsTest, EqualsOperator) {
  TransactionOptions default_options1;
  TransactionOptions default_options2;
  TransactionOptions options1a;
  options1a.set_max_attempts(1);
  TransactionOptions options1b;
  options1b.set_max_attempts(1);
  TransactionOptions options2a;
  options2a.set_max_attempts(99);
  TransactionOptions options2b;
  options2b.set_max_attempts(99);

  EXPECT_TRUE(default_options1 == default_options1);
  EXPECT_TRUE(default_options1 == default_options2);
  EXPECT_TRUE(options1a == options1b);
  EXPECT_TRUE(options2a == options2b);

  EXPECT_FALSE(options1a == options2a);
  EXPECT_FALSE(options1a == default_options1);
  EXPECT_FALSE(options2a == default_options1);
}

TEST(TransactionOptionsTest, NotEqualsOperator) {
  TransactionOptions default_options1;
  TransactionOptions default_options2;
  TransactionOptions options1a;
  options1a.set_max_attempts(1);
  TransactionOptions options1b;
  options1b.set_max_attempts(1);
  TransactionOptions options2a;
  options2a.set_max_attempts(99);
  TransactionOptions options2b;
  options2b.set_max_attempts(99);

  EXPECT_FALSE(default_options1 != default_options1);
  EXPECT_FALSE(default_options1 != default_options2);
  EXPECT_FALSE(options1a != options1b);
  EXPECT_FALSE(options2a != options2b);

  EXPECT_TRUE(options1a != options2a);
  EXPECT_TRUE(options1a != default_options1);
  EXPECT_TRUE(options2a != default_options1);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
