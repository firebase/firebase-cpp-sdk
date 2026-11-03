/*
 * Copyright 2017 Google LLC
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

#include "app/rest/util.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace rest {
namespace util {

TEST(UtilTest, TestTrimWhitespace) {
  // Empty
  EXPECT_EQ("", TrimWhitespace(""));
  // Only white space
  EXPECT_EQ("", TrimWhitespace(" "));
  EXPECT_EQ("", TrimWhitespace(" \r\n \t "));
  // A single letter
  EXPECT_EQ("x", TrimWhitespace(" x"));
  EXPECT_EQ("x", TrimWhitespace("x "));
  EXPECT_EQ("x", TrimWhitespace(" x "));
  // A word
  EXPECT_EQ("abc", TrimWhitespace("\t abc"));
  EXPECT_EQ("abc", TrimWhitespace("abc \r\n"));
  EXPECT_EQ("abc", TrimWhitespace("\t abc \r\n"));
  // A few words
  EXPECT_EQ("mary had little lamb", TrimWhitespace("   mary had little lamb"));
  EXPECT_EQ("mary had little lamb", TrimWhitespace("mary had little lamb   "));
  EXPECT_EQ("mary had little lamb", TrimWhitespace("   mary had little lamb "));
}

TEST(UtilTest, TestToUpper) {
  // Empty
  EXPECT_EQ("", ToUpper(""));
  // Only non-alpha characters
  EXPECT_EQ("3.1415926", ToUpper("3.1415926"));
  // Letters
  EXPECT_EQ("A", ToUpper("a"));
  EXPECT_EQ("ABC", ToUpper("AbC"));
  // Mixed
  EXPECT_EQ("789 ABC", ToUpper("789 abc"));
  EXPECT_EQ("1A2B3C", ToUpper("1a2b3c"));
}

TEST(UtilTest, TestToLower) {
  EXPECT_EQ("", ToLower(""));
  EXPECT_EQ("3.1415926", ToLower("3.1415926"));
  EXPECT_EQ("a", ToLower("A"));
  EXPECT_EQ("abc", ToLower("AbC"));
  EXPECT_EQ("789 abc", ToLower("789 ABC"));
  EXPECT_EQ("1a2b3c", ToLower("1A2B3C"));
}

TEST(UtilTest, TestInitializeEncodeDecodeTerminate) {
  Initialize();
  Initialize();
  EXPECT_EQ("", EncodeUrl(""));
  EXPECT_EQ("", DecodeUrl(""));
  std::string raw = "hello world/foo?bar=1&baz=2+3";
  std::string encoded = EncodeUrl(raw);
  EXPECT_EQ("hello%20world%2Ffoo%3Fbar%3D1%26baz%3D2%2B3", encoded);
  EXPECT_EQ(raw, DecodeUrl(encoded));
  void* ptr = CreateCurlPtr();
  EXPECT_NE(nullptr, ptr);
  DestroyCurlPtr(ptr);
  DestroyCurlPtr(nullptr);
  Terminate();
  Terminate();
}

TEST(UtilTest, TestJsonDataParse) {
  JsonData data;
  EXPECT_FALSE(data.is_valid());
  EXPECT_TRUE(data.Parse("{\"key\":\"value\",\"num\":42}"));
  EXPECT_TRUE(data.is_valid());
  EXPECT_TRUE(data.root().is_map());
}

}  // namespace util
}  // namespace rest
}  // namespace firebase
