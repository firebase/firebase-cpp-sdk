// Copyright 2020 Google
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

#import <Foundation/Foundation.h>

#include <string>

#include "testing/config.h"
#include "testing/ticker.h"
#include "testing/util_ios.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace testing {
namespace cppsdk {

TEST(TickerTest, TestCallbackTicker) {
  TickerReset();
  ConfigSet(
      "{"
      "  config:["
      "    {fake:'a',futuregeneric:{throwexception:false,ticker:1}},"
      "    {fake:'b',futuregeneric:{throwexception:true,exceptionmsg:'failed',ticker:2}},"
      "    {fake:'c',futuregeneric:{throwexception:false,ticker:3}},"
      "    {fake:'d',futuregeneric:{throwexception:true,exceptionmsg:'failed',ticker:4}}"
      "  ]"
      "}");

  __block int count = 0;
  // Now we create four fake objects on the fly; all are managed by manager.
  CallbackTickerManager manager;
  // Without param.
  manager.Add(@"a", ^(NSError* _Nullable error) { if (!error) count++; });
  manager.Add(@"b", ^(NSError* _Nullable error) { if (!error) count++; });
  // With param.
  manager.Add(@"c", ^(NSString* param, NSError* _Nullable error) { if (!error) count++; }, @"par");
  manager.Add(@"d", ^(NSString* param, NSError* _Nullable error) { if (!error) count++; }, @"par");

  // nothing happens so far.
  EXPECT_EQ(0, count);

  // a succeeds and increases counter.
  TickerElapse();
  EXPECT_EQ(1, count);

  // b fails.
  TickerElapse();
  EXPECT_EQ(1, count);

  // c succeeds and increases counter.
  TickerElapse();
  EXPECT_EQ(2, count);

  // d fails.
  TickerElapse();
  EXPECT_EQ(2, count);

  // nothing happens afterwards.
  TickerElapse();
  EXPECT_EQ(2, count);

  TickerReset();
  ConfigReset();
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
