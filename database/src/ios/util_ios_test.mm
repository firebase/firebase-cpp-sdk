/*
 * Copyright 2019 Google LLC
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

#import <XCTest/XCTest.h>

#include "app/src/util_ios.h"

using ::firebase::Variant;
using ::firebase::util::VariantToId;

@interface VariantToIdTests : XCTestCase
@end

@implementation VariantToIdTests

- (void)testNull {
  XCTAssertEqual(VariantToId(Variant::Null()), [NSNull null]);
}

- (void)testInt64WithZero {
  id value_id = VariantToId(Variant::FromInt64(0LL));
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.longLongValue, 0LL);
}

- (void)testInt64WithSigned32BitValue {
  id value_id = VariantToId(Variant::FromInt64(2000000000LL));
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.longLongValue, 2000000000LL);
}

- (void)testInt64WithLongLongValue {
  id value_id = VariantToId(Variant::FromInt64(8000000000LL));
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.longLongValue, 8000000000LL);
}

- (void)testInt64WithLargeValue {
  id value_id = VariantToId(Variant::FromInt64(636900045569749380LL));
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.longLongValue, 636900045569749380LL);
}

- (void)testDoubleWithZeroPointZero {
  id value_id = VariantToId(Variant::ZeroPointZero());
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.doubleValue, 0.0);
}

- (void)testDoubleWithOnePointZero {
  id value_id = VariantToId(Variant::OnePointZero());
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.doubleValue, 1.0);
}

- (void)testDoubleWithPi {
  id value_id = VariantToId(Variant::FromDouble(3.14159));
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.doubleValue, 3.14159);
}

- (void)testBoolWithTrue {
  id value_id = VariantToId(Variant::True());
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.boolValue, true);
}

- (void)testBoolWithFalse {
  id value_id = VariantToId(Variant::False());
  NSNumber *value_number = (NSNumber *)value_id;
  XCTAssertEqual(value_number.boolValue, false);
}

- (void)testStaticStringWithEmptyString {
  id value_id = VariantToId(Variant::FromStaticString(""));
  NSString *value_string = (NSString *)value_id;
  XCTAssertEqualObjects(value_string, @"");
}

- (void)testStaticStringWithNonEmptyString {
  id value_id = VariantToId(Variant::FromStaticString("Hello, world!"));
  NSString *value_string = (NSString *)value_id;
  XCTAssertEqualObjects(value_string, @"Hello, world!");
}

- (void)testMutableStringWithEmptyString {
  id value_id = VariantToId(Variant::FromMutableString(""));
  NSString *value_string = (NSString *)value_id;
  XCTAssertEqualObjects(value_string, @"");
}

- (void)testMutableStringWithNonEmptyString {
  id value_id = VariantToId(Variant::FromMutableString("Hello, world!"));
  NSString *value_string = (NSString *)value_id;
  XCTAssertEqualObjects(value_string, @"Hello, world!");
}

@end
