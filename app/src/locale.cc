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

#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
#error "This file does not support iOS and OS X, use locale_apple.mm instead."
#elif FIREBASE_PLATFORM_ANDROID
#error "This file is not support on Android."
#elif FIREBASE_PLATFORM_WINDOWS
#include <time.h>
#include <windows.h>
#elif FIREBASE_PLATFORM_LINUX
#include <clocale>
#include <ctime>
#include <locale>
#else
#error "Unknown platform."
#endif  // platform selector

#include <algorithm>
#include <vector>

namespace firebase {
namespace internal {

// Get the current locale, e.g. "en_US"
std::string GetLocale() {
#if FIREBASE_PLATFORM_WINDOWS
  LCID lang_id = GetThreadLocale();
  std::vector<wchar_t> locale_name(LOCALE_NAME_MAX_LENGTH);
  if (LCIDToLocaleName(lang_id, &locale_name[0], LOCALE_NAME_MAX_LENGTH, 0) ==
      0) {
    return "";
  }
  std::wstring woutput(&locale_name[0]);
  std::string output(woutput.begin(), woutput.end());
  // Change all hyphens to underscores to normalize the locale.
  std::replace(output.begin(), output.end(), '-', '_');
  return output;
#elif FIREBASE_PLATFORM_LINUX
  // If std::locale() has been customized, return it, else return the contents
  // of the LANG or LC_CTYPE environment variables if set, or otherwise return a
  // default locale (empty in real life, or placeholder when running in a unit
  // test, as the test environment has no locale variables set).
  std::string output = std::locale().name() != "C"
                           ? std::locale().name()
                           : getenv("LANG")
                                 ? getenv("LANG")
                                 : getenv("LC_CTYPE")
                                       ? getenv("LC_CTYPE")
                                       : getenv("TEST_TMPDIR") ? "en_US" : "";
  // Some of the environment variables have a "." after the name to specify the
  // terminal encoding, e.g.  "en_US.UTF-8", so we want to cut the string on the
  // ".".
  size_t cut = output.find(".");
  if (cut != std::string::npos) {
    output = output.substr(0, cut);
  }
  // Do the same with a "@" which some locales also have.
  cut = output.find("@");
  if (cut != std::string::npos) {
    output = output.substr(0, cut);
  }
  return output;
#else
  // An error was already triggered above.
  return "";
#endif  // platform selector
}

// Get the current time zone, e.g. "US/Pacific"
std::string GetTimezone() {
#if FIREBASE_PLATFORM_WINDOWS
  // If "TZ" environment variable is defined, use it, else use _get_tzname.
  int tz_bytes = GetEnvironmentVariable("TZ", nullptr, 0);
  if (tz_bytes > 0) {
    std::vector<char> buffer(tz_bytes);
    GetEnvironmentVariable("TZ", &buffer[0], tz_bytes);
    return std::string(&buffer[0]);
  }
  int daylight;  // daylight savings time?
  if (_get_daylight(&daylight) != 0) return "";
  size_t length = 0;  // get the needed string length
  if (_get_tzname(&length, nullptr, 0, daylight ? 1 : 0) != 0) return "";
  std::vector<char> namebuf(length);
  if (_get_tzname(&length, &namebuf[0], length, daylight ? 1 : 0) != 0)
    return "";
  std::string name_str(&namebuf[0]);
  return name_str;
#elif FIREBASE_PLATFORM_LINUX
  // If TZ environment variable is defined and not empty, use it, else use
  // tzname.
  return (getenv("TZ") && *getenv("TZ")) ? getenv("TZ")
                                         : tzname[daylight ? 1 : 0];
#else
  // An error was already triggered above.
  return "";
#endif  // platform selector
}

}  // namespace internal
}  // namespace firebase
