// Copyright 2018 Google LLC
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

#include "database/tests/desktop/test/matchers.h"

#include <vector>

#include "app/memory/unique_ptr.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Not;
using ::testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {

TEST(SmartPtrRawPtrEq, Matcher) {
  int* five = new int(5);
  EXPECT_THAT(std::make_tuple(UniquePtr<int>(five), five), SmartPtrRawPtrEq());

  int* ten = new int(10);
  int* different_ten = new int(10);
  EXPECT_THAT(std::make_tuple(UniquePtr<int>(ten), different_ten),
              Not(SmartPtrRawPtrEq()));
  delete different_ten;
}

TEST(SmartPtrRawPtrEq, Pointwise) {
  int* five = new int(5);
  int* ten = new int(10);
  int* fifteen = new int(15);
  int* twenty = new int(20);
  int* different_twenty = new int(20);
  std::vector<UniquePtr<int>> unique_values{
      UniquePtr<int>(five),
      UniquePtr<int>(ten),
      UniquePtr<int>(fifteen),
      UniquePtr<int>(twenty),
  };
  std::vector<int*> raw_values{
      five,
      ten,
      fifteen,
      twenty,
  };
  std::vector<int*> wrong_raw_values{
      five,
      ten,
      fifteen,
      different_twenty,
  };
  EXPECT_THAT(unique_values, Pointwise(SmartPtrRawPtrEq(), raw_values));
  EXPECT_THAT(unique_values,
              Not(Pointwise(SmartPtrRawPtrEq(), wrong_raw_values)));
  delete different_twenty;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
