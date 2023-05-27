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

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#error "This file does not support iOS and OS X, use locale_apple.mm instead."
#elif FIREBASE_PLATFORM_ANDROID
#error "This file is not support on Android."
#elif FIREBASE_PLATFORM_WINDOWS
#include <time.h>
#include <windows.h>
// To convert Windows time zone names to IANA time zone names:
#define UCHAR_TYPE wchar_t
#include <icu.h>
#elif FIREBASE_PLATFORM_LINUX
#include <clocale>
#include <ctime>
#else
#error "Unknown platform."
#endif  // platform selector

#include <algorithm>
#include <codecvt>
#include <locale>
#include <string>
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
  LogInfo("Locale: %s", output.c_str());
  return output;
#elif FIREBASE_PLATFORM_LINUX
  // If std::locale() has been customized, return it, else return the contents
  // of the LANG or LC_CTYPE environment variables if set, or otherwise return a
  // default locale (empty in real life, or placeholder when running in a unit
  // test, as the test environment has no locale variables set).
  // clang-format off
  std::string output = std::locale().name() != "C"
                           ? std::locale().name()
                           : getenv("LANG")
                                 ? getenv("LANG")
                                 : getenv("LC_CTYPE")
                                       ? getenv("LC_CTYPE")
                                       : getenv("TEST_TMPDIR") ? "en_US" : "";
  // clang-format on
  // Some of the environment variables have a "." after the name to specify the
  // terminal encoding, e.g.  "en_US.UTF-8", so we want to cut the string on the
  // ".".
  size_t cut = output.find('.');
  if (cut != std::string::npos) {
    output = output.substr(0, cut);
  }
  // Do the same with a "@" which some locales also have.
  cut = output.find('@');
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
  static bool tz_was_set = false;
  if (!tz_was_set) {
    _tzset();  // Set the time zone used by the below functions, based on OS
               // settings or the TZ variable, as appropriate.
    tz_was_set = true;
  }
  int daylight;  // daylight savings time?
  if (_get_daylight(&daylight) != 0) return "";
  size_t length = 0;  // get the needed string length
  if (_get_tzname(&length, nullptr, 0, daylight ? 1 : 0) != 0) return "";
  std::vector<char> namebuf(length);
  if (_get_tzname(&length, &namebuf[0], length, daylight ? 1 : 0) != 0)
    return "";
  std::string windows_tz_utf8(&namebuf[0]);
  LogInfo("Windows time zone: %s", windows_tz_utf8.c_str());

  // Convert time zone name to wide string
  std::wstring_convert<std::codecvt_utf8<wchar_t>> to_utf16;
  std::wstring windows_tz_utf16 = to_utf16.from_bytes(windows_tz_utf8);

  std::string locale_name = GetLocale();
  wchar_t iana_time_zone_buffer[128];
  UErrorCode error_code;
  int32_t size;
  bool got_time_zone = false;
  if (locale_name.size() >= 5) {
    // Try time zone first with the region code returned above, assuming it's at
    // least 5 characters. For example, "en_US" -> "US"
    std::string region_code = std::string(&locale_name[3], 2);
    size = ucal_getTimeZoneIDForWindowsID(
        windows_tz_utf16.c_str(), -1, region_code.c_str(),
        iana_time_zone_buffer,
        sizeof(iana_time_zone_buffer) / sizeof(iana_time_zone_buffer[0]),
        &error_code);
    got_time_zone = (U_SUCCESS(error_code) && size > 0);
    if (!got_time_zone) {
      LogWarning(
          "Couldn't convert Windows time zone '%s' with region '%s' to IANA: "
          "%d",
          windows_tz_utf8.c_str(), region_code.c_str(), error_code);
    }
  }
  if (!got_time_zone) {
    // Try without specifying a region
    size = ucal_getTimeZoneIDForWindowsID(
        windows_tz_utf16.c_str(), -1, nullptr, iana_time_zone_buffer,
        sizeof(iana_time_zone_buffer) / sizeof(iana_time_zone_buffer[0]),
        &error_code);
    got_time_zone = (U_SUCCESS(error_code) && size > 0);
    if (!got_time_zone) {
      // Couldn't convert to IANA
      LogError("Couldn't convert time zone '%s' to IANA: %d",
               windows_tz_utf8.c_str(), error_code);
    }
  }
  if (!got_time_zone) {
    // Return the Windows time zone ID as a backup.
    return windows_tz_utf8;
  }

  std::wstring iana_tz_utf16(iana_time_zone_buffer);
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> to_utf8;
  std::string iana_tz_utf8 = to_utf8.to_bytes(tz_utf16);
  LogInfo("IANA time zone: %s", iana_tz_utf8.c_str());
  return iana_tz_utf8;

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
