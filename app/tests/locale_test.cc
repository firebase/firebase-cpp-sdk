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

#include <string>

#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#if FIREBASE_PLATFORM_WINDOWS

#else
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
  EXPECT_NE(loc.find("_"), std::string::npos);
}

}  // namespace internal
}  // namespace firebase
