/*
 * Copyright 2026 Google LLC
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

#include "app/src/include/firebase/internal/type_traits.h"

#include <cstdint>
#include <string>

#include "gtest/gtest.h"

namespace firebase {
namespace {

// Helper taking const bool& to exercise ODR-use of trait ::value members.
bool IdentityRef(const bool& v) { return v; }

TEST(TypeTraitsTest, RemoveReference) {
  EXPECT_TRUE((is_same<typename remove_reference<int>::type, int>::value));
  EXPECT_TRUE((is_same<typename remove_reference<int&>::type, int>::value));
  EXPECT_TRUE((is_same<typename remove_reference<int&&>::type, int>::value));
}

TEST(TypeTraitsTest, IsArray) {
  EXPECT_FALSE(IdentityRef(is_array<int>::value));
  EXPECT_TRUE(IdentityRef(is_array<int[]>::value));
  EXPECT_TRUE(IdentityRef(is_array<int[4]>::value));
}

TEST(TypeTraitsTest, IsLvalueReference) {
  EXPECT_FALSE(IdentityRef(is_lvalue_reference<int>::value));
  EXPECT_TRUE(IdentityRef(is_lvalue_reference<int&>::value));
  EXPECT_FALSE(IdentityRef(is_lvalue_reference<int&&>::value));
}

TEST(TypeTraitsTest, IsChar) {
  EXPECT_TRUE(IdentityRef(is_char<char>::value));
  EXPECT_TRUE(IdentityRef(is_char<const char>::value));
  EXPECT_TRUE(IdentityRef(is_char<volatile char>::value));
  EXPECT_TRUE(IdentityRef(is_char<const volatile char>::value));
  EXPECT_TRUE(IdentityRef(is_char<signed char>::value));
  EXPECT_TRUE(IdentityRef(is_char<unsigned char>::value));
  EXPECT_TRUE(IdentityRef(is_char<wchar_t>::value));
  EXPECT_TRUE(IdentityRef(is_char<char16_t>::value));
  EXPECT_TRUE(IdentityRef(is_char<char32_t>::value));
#if defined(__cpp_char8_t)
  EXPECT_TRUE(IdentityRef(is_char<char8_t>::value));
  EXPECT_TRUE(IdentityRef(is_char<const char8_t>::value));
#endif
  EXPECT_FALSE(IdentityRef(is_char<bool>::value));
  EXPECT_FALSE(IdentityRef(is_char<int>::value));
  EXPECT_FALSE(IdentityRef(is_char<char*>::value));
  EXPECT_FALSE(IdentityRef(is_char<std::string>::value));
}

TEST(TypeTraitsTest, IsInteger) {
  EXPECT_TRUE(IdentityRef(is_integer<int16_t>::value));
  EXPECT_TRUE(IdentityRef(is_integer<int>::value));
  EXPECT_TRUE(IdentityRef(is_integer<const int>::value));
  EXPECT_TRUE(IdentityRef(is_integer<volatile int>::value));
  EXPECT_TRUE(IdentityRef(is_integer<const volatile int>::value));
  EXPECT_TRUE(IdentityRef(is_integer<int64_t>::value));
  EXPECT_TRUE(IdentityRef(is_integer<uint64_t>::value));

  EXPECT_FALSE(IdentityRef(is_integer<bool>::value));
  EXPECT_FALSE(IdentityRef(is_integer<const bool>::value));
  EXPECT_FALSE(IdentityRef(is_integer<volatile bool>::value));
  EXPECT_FALSE(IdentityRef(is_integer<const volatile bool>::value));
  EXPECT_FALSE(IdentityRef(is_integer<char>::value));
  EXPECT_FALSE(IdentityRef(is_integer<const char>::value));
  EXPECT_FALSE(IdentityRef(is_integer<volatile char>::value));
  EXPECT_FALSE(IdentityRef(is_integer<const volatile char>::value));
  EXPECT_FALSE(IdentityRef(is_integer<signed char>::value));
  EXPECT_FALSE(IdentityRef(is_integer<unsigned char>::value));
  EXPECT_FALSE(IdentityRef(is_integer<wchar_t>::value));
  EXPECT_FALSE(IdentityRef(is_integer<char16_t>::value));
  EXPECT_FALSE(IdentityRef(is_integer<char32_t>::value));
#if defined(__cpp_char8_t)
  EXPECT_FALSE(IdentityRef(is_integer<char8_t>::value));
  EXPECT_FALSE(IdentityRef(is_integer<const char8_t>::value));
#endif
  EXPECT_FALSE(IdentityRef(is_integer<double>::value));
}

}  // namespace
}  // namespace firebase
