#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include "testing/base/public/gunit.h"

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
