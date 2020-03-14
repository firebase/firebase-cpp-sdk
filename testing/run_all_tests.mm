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
#import <XCTest/XCTest.h>

#include "gtest/gtest.h"

@interface RunAllTests : XCTestCase
@end

@implementation RunAllTests

- (void)testAll {
  int argc = 1;
  const char* argv[] = {"ios-test-wrapper"};
  testing::InitGoogleTest(&argc, const_cast<char**>(argv));

  int result = RUN_ALL_TESTS();

  // Log test summary.
  testing::UnitTest* unit_test = testing::UnitTest::GetInstance();
  NSLog(@"Tests finished.\n  passed tests: %d\n  failed tests: %d\n"
         "  disabled tests: %d\n  total tests: %d",
        unit_test->successful_test_count(),
        unit_test->failed_test_count(),
        unit_test->disabled_test_count(),
        unit_test->total_test_count());

  // Run test could succeed trivially if the test case is not linked.
  XCTAssertGreaterThan(unit_test->total_test_count(), 0);

  // No test case fails.
  XCTAssertEqual(result, 0);
}

@end
