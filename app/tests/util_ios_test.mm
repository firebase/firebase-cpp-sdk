/*
 * Copyright 2018 Google LLC
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

#include "app/src/include/firebase/variant.h"
#include "app/src/util_ios.h"

typedef firebase::util::ObjCPointer<NSString> NSStringCpp;
OBJ_C_PTR_WRAPPER_NAMED(NSStringHandle, NSString);
OBJ_C_PTR_WRAPPER(NSString);

@interface ObjCPointerTests : XCTestCase
@end

@implementation ObjCPointerTests

- (void)testConstructAndGet {
  NSStringCpp cpp;
  NSStringHandle handle;
  NSStringPointer pointer;
  XCTAssertEqual(cpp.get(), nil);
  XCTAssertEqual(handle.get(), nil);
  XCTAssertEqual(pointer.get(),  nil);
}

- (void)testConstructWithObjectAndGet {
  NSString* nsstring = @"hello";
  NSStringCpp cpp(nsstring);
  NSStringHandle handle(nsstring);
  NSStringPointer pointer(nsstring);
  NSStringHandle from_base_type(cpp);
  XCTAssertEqual(cpp.get(), nsstring);
  XCTAssertEqual(handle.get(), nsstring);
  XCTAssertEqual(pointer.get(),  nsstring);
  XCTAssertEqual(from_base_type.get(), nsstring);
}

- (void)testRelease {
  NSString *nsstring = @"hello";
  NSStringCpp cpp(nsstring);
  XCTAssertEqual(cpp.get(), nsstring);
  cpp.release();
  XCTAssertEqual(cpp.get(), nil);
}

- (void)testBoolOperator {
  NSStringCpp cpp(@"hello");
  XCTAssertTrue(cpp);
  cpp.release();
  XCTAssertFalse(cpp);
}

- (void)testReset {
  NSString* hello = @"hello";
  NSString* goodbye = @"goodbye";
  NSStringCpp cpp(hello);
  XCTAssertEqual(cpp.get(), hello);
  cpp.reset(goodbye);
  XCTAssertEqual(cpp.get(), goodbye);
}

- (void)testAssign {
  NSString* hello = @"hello";
  NSString* goodbye = @"goodbye";
  NSStringCpp cpp(hello);
  NSStringHandle handle(hello);
  NSStringPointer pointer(hello);
  XCTAssertEqual(cpp.get(), hello);
  XCTAssertEqual(*cpp, hello);
  XCTAssertEqual(handle.get(), hello);
  XCTAssertEqual(*handle, hello);
  XCTAssertEqual(pointer.get(), hello);
  XCTAssertEqual(*pointer, hello);
  XCTAssertEqual((*cpp).length, 5);
  XCTAssertEqual((*handle).length, 5);
  XCTAssertEqual((*pointer).length, 5);
  cpp = goodbye;
  handle = goodbye;
  pointer = goodbye;
  XCTAssertEqual(cpp.get(), goodbye);
  XCTAssertEqual(handle.get(), goodbye);
  XCTAssertEqual(pointer.get(), goodbye);
}

- (void)testSafeGet {
  NSString* hello = @"hello";
  NSStringCpp cpp(hello);
  NSStringHandle handle(hello);
  NSStringPointer pointer(hello);
  XCTAssertEqual(NSStringCpp::SafeGet(&cpp), hello);
  XCTAssertEqual(NSStringHandle::SafeGet(&handle), hello);
  XCTAssertEqual(NSStringPointer::SafeGet(&pointer), hello);
  cpp.release();
  handle.release();
  pointer.release();
  XCTAssertEqual(NSStringCpp::SafeGet(&cpp), nil);
  XCTAssertEqual(NSStringHandle::SafeGet(&handle), nil);
  XCTAssertEqual(NSStringPointer::SafeGet(&pointer), nil);
  XCTAssertEqual(NSStringCpp::SafeGet(nullptr), nil);
}

@end

using ::firebase::Variant;
using ::firebase::util::IdToVariant;
using ::firebase::util::VariantToId;

@interface IdToVariantTests : XCTestCase
@end

@implementation IdToVariantTests

- (void)testNil {
  // Check that nil maps to a null variant and that a non-nil value does not map
  // to a null variant.
  {
    // Nil id.
    id value = nil;
    Variant variant = IdToVariant(value);
    XCTAssertTrue(variant.is_null());
  }
  {
    // Non-nil id.
    id value = [NSNumber numberWithInteger:0];
    Variant variant = IdToVariant(value);
    XCTAssertFalse(variant.is_null());
  }
}

- (void)testInteger {
  // Check that integers map to the correct variant, even when those numbers
  // exceed the maximum integer value.
  {
    // Check that the integer 0 maps to an integer variant holding 0.
    id number = [NSNumber numberWithInteger:0];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_int64());
    XCTAssertTrue(variant.int64_value() == 0);
  }
  {
    // Check that the integer 1 maps to an integer variant holding 1.
    id number = [NSNumber numberWithInteger:1];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_int64());
    XCTAssertTrue(variant.int64_value() == 1);
  }
  {
    // Check that the integer 10 maps to an integer variant holding 10.
    id number = [NSNumber numberWithInteger:10];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_int64());
    XCTAssertTrue(variant.int64_value() == 10);
  }
  {
    // Check that a variant can hander an integer larger than the largest 32 bit
    // int.
    id number = [NSNumber numberWithInteger:5000000000ll];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_int64());
    XCTAssertTrue(variant.int64_value() == 5000000000ll);
  }
}

- (void)testDouble {
  // Check that integers map to the correct variant, even when those numbers
  // exceed the maximum 64 bit integer value.
  {
    // Check that the double 0.0 maps to a double variant holding 0.0.
    id number = [NSNumber numberWithDouble:0.0];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_double());
    XCTAssertTrue(variant.double_value() == 0.0);
  }
  {
    // Check that a variant can hander fractional values.
    id number = [NSNumber numberWithDouble:0.5];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_double());
    XCTAssertTrue(variant.double_value() == 0.5);
  }
  {
    // Check that the double 1.0 maps to a double variant holding 1.0.
    id number = [NSNumber numberWithDouble:1.0];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_double());
    XCTAssertTrue(variant.double_value() == 1.0);
  }
  {
    // Check that the double 10.0 maps to a double variant holding 10.0.
    id number = [NSNumber numberWithDouble:10.0];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_double());
    XCTAssertTrue(variant.double_value() == 10.0);
  }
  {
    // Check that a variant can hander a double larger than the largest 32 bit
    // int.
    id number = [NSNumber numberWithDouble:5000000000.0];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_double());
    XCTAssertTrue(variant.double_value() == 5000000000.0);
  }
  {
    // Check that a variant can hander a double larger than the largest 64 bit
    // int.
    id number = [NSNumber numberWithDouble:20000000000000000000.0];
    Variant variant = IdToVariant(number);
    XCTAssertTrue(variant.is_double());
    XCTAssertTrue(variant.double_value() == 20000000000000000000.0);
  }
}

- (void)testBool {
  // Check that boolean values map to the correct boolean variant.
  {
    id value = [NSNumber numberWithBool:YES];
    Variant variant = IdToVariant(value);
    XCTAssertTrue(variant.is_bool());
    XCTAssertTrue(variant.bool_value() == true);
  }
  {
    id value = [NSNumber numberWithBool:NO];
    Variant variant = IdToVariant(value);
    XCTAssertTrue(variant.is_bool());
    XCTAssertTrue(variant.bool_value() == false);
  }
}

- (void)testString {
  // Check that NSStrings map to the correct std::string variants.
  {
    // Empty string.
    id str = @"";
    Variant variant = IdToVariant(str);
    XCTAssertTrue(variant.is_string());
    XCTAssertTrue(variant.is_mutable_string());
    XCTAssertTrue(variant.string_value() == std::string(""));
  }
  {
    // Non-empty string.
    id str = @"Test With Very Very Long String";
    Variant variant = IdToVariant(str);
    XCTAssertTrue(variant.is_string());
    XCTAssertTrue(variant.is_mutable_string());
    XCTAssertTrue(variant.string_value() == std::string("Test With Very Very Long String"));
  }
}

- (void)testVector {
  // Check that NSArrays map to the correct vector variants.
  {
    // Empty NSArray to empty vector.
    id array = @[];
    std::vector<Variant> expected;
    Variant variant = IdToVariant(array);
    XCTAssertTrue(variant.is_vector());
    XCTAssertTrue(variant.vector() == expected);
  }
  {
    // NSArray of numbers to vector of integer variants.
    id array = @[ @1, @2, @3, @4, @5 ];
    std::vector<Variant> expected{1, 2, 3, 4, 5};
    Variant variant = IdToVariant(array);
    XCTAssertTrue(variant.is_vector());
    XCTAssertTrue(variant.vector() == expected);
  }
  {
    // NSArray of NSStrings to vector of std::string variants.
    id array = @[ @"This", @"is", @"a", @"test." ];
    std::vector<Variant> expected{"This", "is", "a", "test."};
    Variant variant = IdToVariant(array);
    XCTAssertTrue(variant.is_vector());
    XCTAssertTrue(variant.vector() == expected);
  }
  {
    // NSArray of various types to vector of variants holding varying types.
    id array = @[ @"Different types", @10, @3.14 ];
    std::vector<Variant> expected{std::string("Different types"), 10, 3.14};
    Variant variant = IdToVariant(array);
    XCTAssertTrue(variant.is_vector());
    XCTAssertTrue(variant.vector() == expected);
  }
  {
    // NSArray containing an NSArray and an NSDictionary to an std::vector
    // holding an std::vector and std::map
    id array = @[ @[ @1, @2, @3 ], @{ @4 : @5, @6 : @7, @8 : @9 } ];
    std::vector<Variant> vector_element{1, 2, 3};
    std::map<Variant, Variant> map_element{
        std::make_pair(Variant(4), Variant(5)),
        std::make_pair(Variant(6), Variant(7)),
        std::make_pair(Variant(8), Variant(9))};
    std::vector<Variant> expected{Variant(vector_element),
                                  Variant(map_element)};
    Variant variant = IdToVariant(array);
    XCTAssertTrue(variant.is_vector());
    XCTAssertTrue(variant.vector() == expected);
  }
}

- (void)testMap {
  {
    // Check that an empty NSDictionary maps to an empty std::map.
    id dictionary = @{};
    std::map<Variant, Variant> expected;
    Variant variant = IdToVariant(dictionary);
    XCTAssertTrue(variant.is_map());
    XCTAssertTrue(variant.map() == expected);
  }
  {
    // Check that a NSDictionary of strings to numbers maps to a std::map of
    // string variants to number variants.
    id dictionary = @{
      @"test1" : @1,
      @"test2" : @2,
      @"test3" : @3,
      @"test4" : @4,
      @"test5" : @5
    };
    std::map<Variant, Variant> expected{
        std::make_pair(Variant("test1"), Variant(1)),
        std::make_pair(Variant("test2"), Variant(2)),
        std::make_pair(Variant("test3"), Variant(3)),
        std::make_pair(Variant("test4"), Variant(4)),
        std::make_pair(Variant("test5"), Variant(5))};
    Variant variant = IdToVariant(dictionary);
    XCTAssertTrue(variant.is_map());
    XCTAssertTrue(variant.map() == expected);
  }
  {
    // Check that a NSDictionary of various types maps to a std::map of variants
    // holding various types.
    id dictionary = @{ @20 : @"Different types", @6.28 : @10, @"Blah" : @3.14 };
    std::map<Variant, Variant> expected{
        std::make_pair(Variant(20), Variant("Different types")),
        std::make_pair(Variant(6.28), Variant(10)),
        std::make_pair(Variant("Blah"), Variant(3.14))};
    Variant variant = IdToVariant(dictionary);
    XCTAssertTrue(variant.is_map());
    XCTAssertTrue(variant.map() == expected);
  }
  {
    // Check that a NSDictionary of NSArray-to-NSDictionary maps to an std::map
    // of vector-to-map
    id dictionary = @{ @[ @1, @2, @3 ] : @{@4 : @5, @6 : @7, @8 : @9} };
    std::vector<Variant> vector_element{1, 2, 3};
    std::map<Variant, Variant> map_element{
        std::make_pair(Variant(4), Variant(5)),
        std::make_pair(Variant(6), Variant(7)),
        std::make_pair(Variant(8), Variant(9))};
    std::map<Variant, Variant> expected{
        std::make_pair(Variant(vector_element), Variant(map_element))};
    Variant variant = IdToVariant(dictionary);
    XCTAssertTrue(variant.is_map());
    XCTAssertTrue(variant.map() == expected);
  }
}

@end

@interface VariantToIdTests : XCTestCase
@end

@implementation VariantToIdTests

- (void)testNil {
  // Check that null variant maps to nil variant and that a non-null does not
  // map to a nil id.
  {
    // Null variant.
    Variant variant;
    id value = VariantToId(variant);
    XCTAssertTrue(value == [NSNull null]);
  }
  {
    // Non-null variant.
    Variant variant(10);
    id value = VariantToId(variant);
    XCTAssertTrue(value != [NSNull null]);
  }
}

- (void)testInteger {
  // Check that integers map to the correct variant, even when those numbers
  // exceed the maximum integer value.
  {
    // Check that the variant 0 maps to an NSNumber holding 0.
    Variant variant(0);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number longLongValue] == 0);
  }
  {
    // Check that the variant 1 maps to an NSNumber holding 1.
    Variant variant(1);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number longLongValue] == 1);
  }
  {
    // Check that the variant 10 maps to an NSNumber holding 10.
    Variant variant(10);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number longLongValue] == 10);
  }
  {
    // Check that a variant can hander an integer larger than the largest 32 bit
    // int.
    Variant variant(5000000000ll);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number longLongValue] == 5000000000ll);
  }
}

- (void)testDouble {
  // Check that doubles map to the correct variant, even when those numbers
  // exceed the maximum integer value.
  {
    // Check that the variant 0.0 maps to an NSNumber holding 0.0.
    Variant variant(0.0);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number doubleValue] == 0.0);
  }
  {
    // Check that a variant can hander fractional values.
    Variant variant(0.5);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number doubleValue] == 0.5);
  }
  {
    // Check that the variant 1.0 maps to an NSNumber holding 1.0.
    Variant variant(1.0);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number doubleValue] == 1.0);
  }
  {
    // Check that the variant 10.0 maps to an NSNumber holding 10.0.
    Variant variant(10.0);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number doubleValue] == 10.0);
  }
  {
    // Check that a variant can hander a double larger than the largest 32 bit
    // int.
    Variant variant(5000000000.0);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number doubleValue] == 5000000000.0);
  }
  {
    // Check that a variant can hander a double larger than the largest 64 bit
    // int.
    Variant variant(20000000000000000000.0);
    id number = VariantToId(variant);
    XCTAssertTrue([number isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([number doubleValue] == 20000000000000000000.0);
  }
}

- (void)testBool {
  // Check that boolean variants map to the correct NSNumbers.
  {
    Variant variant(true);
    id value = VariantToId(variant);
    XCTAssertTrue([value isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([value boolValue] == YES);
  }
  {
    Variant variant(false);
    id value = VariantToId(variant);
    XCTAssertTrue([value isKindOfClass:[NSNumber class]]);
    XCTAssertTrue([value boolValue] == NO);
  }
}

- (void)testString {
  {
    // Empty static string.
    const char* input_string = "";
    Variant variant(input_string);
    id value = VariantToId(variant);
    XCTAssertTrue([value isKindOfClass:[NSString class]]);
    XCTAssertTrue([value isEqualToString:@""]);
  }
  {
    // Empty mutable string.
    std::string input_string = "";
    Variant variant(input_string);
    id value = VariantToId(variant);
    XCTAssertTrue([value isKindOfClass:[NSString class]]);
    XCTAssertTrue([value isEqualToString:@""]);
  }
  {
    // Non-empty static string.
    const char* input_string = "Test";
    Variant variant(input_string);
    id value = VariantToId(variant);
    XCTAssertTrue([value isKindOfClass:[NSString class]]);
    XCTAssertTrue([value isEqualToString:@"Test"]);
  }
  {
    // Non-empty mutable string.
    std::string input_string = "Test";
    Variant variant(input_string);
    id value = VariantToId(variant);
    XCTAssertTrue([value isKindOfClass:[NSString class]]);
    XCTAssertTrue([value isEqualToString:@"Test"]);
  }
}

- (void)testVector {
  // Check that std::vectors map to NSArrays, even when those numbers
  // exceed the maximum integer value.
  {
    // Empty std::vector to empty NSArray.
    std::vector<Variant> vector;
    id expected = @[];
    Variant variant(vector);
    id array = VariantToId(variant);
    XCTAssertTrue([array isKindOfClass:[NSArray class]]);
    XCTAssertTrue([array isEqual:expected]);
  }
  {
    // std::vector of integers to NSArray of NSNumbers.
    std::vector<Variant> vector{1, 2, 3, 4, 5};
    id expected = @[ @1, @2, @3, @4, @5 ];
    Variant variant(vector);
    id array = VariantToId(variant);
    XCTAssertTrue([array isKindOfClass:[NSArray class]]);
    XCTAssertTrue([array isEqual:expected]);
  }
  {
    // std::vector of static and mutable strings to NSArray of NSStrings.
    std::vector<Variant> vector{"This", std::string("is"), "a",
                                std::string("test.")};
    id expected = @[ @"This", @"is", @"a", @"test." ];
    Variant variant(vector);
    id array = VariantToId(variant);
    XCTAssertTrue([array isKindOfClass:[NSArray class]]);
    XCTAssertTrue([array isEqual:expected]);
  }
  {
    // std::vector of various types to NSArray of various types.
    std::vector<Variant> vector{"Different types", 10, 3.14};
    id expected = @[ @"Different types", @10, @3.14 ];
    Variant variant(vector);
    id array = VariantToId(variant);
    XCTAssertTrue([array isKindOfClass:[NSArray class]]);
    XCTAssertTrue([array isEqual:expected]);
  }
  {
    // std::vector containing a vector and map to an NSArray containing an
    // NSArray and an NSDictionary.
    std::vector<Variant> vector_element{1, 2, 3};
    std::map<Variant, Variant> map_element{
        std::make_pair(Variant(4), Variant(5)),
        std::make_pair(Variant(6), Variant(7)),
        std::make_pair(Variant(8), Variant(9))};
    std::vector<Variant> vector{Variant(vector_element), Variant(map_element)};
    id expected = @[ @[ @1, @2, @3 ], @{ @4 : @5, @6 : @7, @8 : @9 } ];
    Variant variant(vector);
    id array = VariantToId(variant);
    XCTAssertTrue([array isKindOfClass:[NSArray class]]);
    XCTAssertTrue([array isEqual:expected]);
  }
}

- (void)testMap {
  // Check that an std::maps map to NSDictionarys with correct types.
  {
    // Check that empty std::map maps to an empty NSDictionary.
    std::map<Variant, Variant> map;
    id expected = @{};
    Variant variant(map);
    id dictionary = VariantToId(variant);
    XCTAssertTrue([dictionary isKindOfClass:[NSDictionary class]]);
    XCTAssertTrue([dictionary isEqual:expected]);
  }
  {
    // Check that an std::map of strings to numbers maps to an NSDictionary of
    // NSString to NSNumbers.
    std::map<Variant, Variant> map{
        std::make_pair(Variant("test1"), Variant(1)),
        std::make_pair(Variant("test2"), Variant(2)),
        std::make_pair(Variant("test3"), Variant(3)),
        std::make_pair(Variant("test4"), Variant(4)),
        std::make_pair(Variant("test5"), Variant(5))};
    id expected = @{
      @"test1" : @1,
      @"test2" : @2,
      @"test3" : @3,
      @"test4" : @4,
      @"test5" : @5
    };
    Variant variant(map);
    id dictionary = VariantToId(variant);
    XCTAssertTrue([dictionary isKindOfClass:[NSDictionary class]]);
    XCTAssertTrue([dictionary isEqual:expected]);
  }
  {
    // Check that an std::map of various types maps to an NSDictionary of
    // various types.
    std::map<Variant, Variant> map{
        std::make_pair(Variant(20), Variant(std::string("Different types"))),
        std::make_pair(Variant(6.28), Variant(10)),
        std::make_pair(Variant("Blah"), Variant(3.14))};
    id expected = @{ @20 : @"Different types", @6.28 : @10, @"Blah" : @3.14 };
    Variant variant(map);
    id dictionary = VariantToId(variant);
    XCTAssertTrue([dictionary isKindOfClass:[NSDictionary class]]);
    XCTAssertTrue([dictionary isEqual:expected]);
  }
  {
    // Check that an std::map of vector-to-map maps to a NSDictionary of
    // NSArray-to-NSDictionary
    std::vector<Variant> vector_element{1, 2, 3};
    std::map<Variant, Variant> map_element{
        std::make_pair(Variant(4), Variant(5)),
        std::make_pair(Variant(6), Variant(7)),
        std::make_pair(Variant(8), Variant(9))};
    std::map<Variant, Variant> map{
        std::make_pair(Variant(vector_element), Variant(map_element))};
    id expected = @{ @[ @1, @2, @3 ] : @{@4 : @5, @6 : @7, @8 : @9} };
    Variant variant(map);
    id dictionary = VariantToId(variant);
    XCTAssertTrue([dictionary isKindOfClass:[NSDictionary class]]);
    XCTAssertTrue([dictionary isEqual:expected]);
  }
}

@end
