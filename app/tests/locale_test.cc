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

#include "app/src/locale.h"

#include <codecvt>
#include <string>

#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#if !FIREBASE_PLATFORM_WINDOWS
#include <clocale>
#endif  // FIREBASE_PLATFORM_WINDOWS

namespace firebase {
namespace internal {

class LocaleTest : public ::testing::Test {};

TEST_F(LocaleTest, TestGetTimezone) {
  std::string tz = GetTimezone();
  LogInfo("GetTimezone() returned '%s'", tz.c_str());
  // There is not a set format for timezones, so we must assume success if it
  // was non-empty.
  EXPECT_NE(tz, "");
}

TEST_F(LocaleTest, TestGetLocale) {
  std::string loc = GetLocale();
  LogInfo("GetLocale() returned '%s'", loc.c_str());
  EXPECT_NE(loc, "");
  // Make sure this looks like a locale, e.g. has at least 5 characters and
  // contains an underscore.
  EXPECT_GE(loc.size(), 5);
  EXPECT_NE(loc.find('_'), std::string::npos);
}

#if FIREBASE_PLATFORM_WINDOWS || FIREBASE_PLATFORM_LINUX

TEST_F(LocaleTest, TestConvertingStringEncodings) {
  // "Mitteleuropäische Zeit € d'été ‘Žœ’" in CP-1252 encoding
  const unsigned char original_cp1252_array[] = {
      'M', 'i', 't',  't',  'e', 'l',  'e', 'u',  'r',  'o',  'p',  0xe4,
      'i', 's', 'c',  'h',  'e', ' ',  'Z', 'e',  'i',  't',  ' ',  0x80,
      ' ', 'd', '\'', 0xe9, 't', 0xe9, ' ', 0x91, 0x8e, 0x9c, 0x92, '\0'};
  std::string original_cp1252(
      reinterpret_cast<const char*>(original_cp1252_array));

  // "Mitteleuropäische Zeit €" in UTF-16 encoding
  const wchar_t original_utf16_array[] =  {
	  'M','i','t','t','e','l','e','u','r','o','p',0x00E4,
	  'i','s','c','h','e',' ','Z','e','i','t',' ',0x20AC,
	  ' ','d','\'',0x00E9,'t',0x00E9,' ',0x2018,0x17D,0x153,0x2019,
	  '\0'};
  std::wstring original_utf16(reinterpret_cast<const wchar_t*>(original_utf16_array));
  

  std::wstring cp1252_converted_to_utf16 =
      convert_cp1252_to_utf16(original_cp1252.c_str());
  EXPECT_EQ(cp1252_converted_to_utf16, original_utf16);

  const unsigned char original_utf8_array[] = {
      'M',  'i',  't',  't',  'e',  'l',  'e',  'u',  'r',  'o',  'p',  0xc3,
      0xa4, 'i',  's',  'c',  'h',  'e',  ' ',  'Z',  'e',  'i',  't',  ' ',
      0xe2, 0x82, 0xac, ' ',  'd',  '\'', 0xc3, 0xa9, 't',  0xc3, 0xa9, ' ',
      0xe2, 0x80, 0x98, 0xc5, 0xbd, 0xc5, 0x93, 0xe2, 0x80, 0x99, '\0'};
  std::string original_utf8(reinterpret_cast<const char*>(original_utf8_array));
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> to_utf8;

  std::string utf16_converted_to_utf8 =
      to_utf8.to_bytes(cp1252_converted_to_utf16);
  EXPECT_EQ(utf16_converted_to_utf8, original_utf8);

  std::wstring utf8_converted_to_utf16 =
      to_utf8.from_bytes(utf16_converted_to_utf8);
  EXPECT_EQ(utf8_converted_to_utf16, original_utf16);
}

#endif  // FIREBASE_PLATFORM_WINDOWS || FIREBASE_PLATFORM_LINUX

}  // namespace internal
}  // namespace firebase
