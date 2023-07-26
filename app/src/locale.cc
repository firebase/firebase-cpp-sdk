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
#include <stdio.h>

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

#include "app/src/thread.h"

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

#if FIREBASE_PLATFORM_WINDOWS
std::wstring GetWindowsTimezoneInEnglish(int daylight) {
  struct TzNames {
    std::wstring standard;
    std::wstring daylight;
  } tz_names;
  Thread thread(
      [](TzNames* tz_names_ptr) {
        SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
        TIME_ZONE_INFORMATION tzi;
        GetTimeZoneInformation(&tzi);
        tz_names_ptr->standard = tzi.StandardName;
        tz_names_ptr->daylight = tzi.DaylightName;
      },
      &tz_names);
  thread.Join();

  return daylight ? tz_names.daylight : tz_names.standard;
}
#endif  // FIREBASE_PLATFORM_WINDOWS

// Get the current time zone, e.g. "US/Pacific"
std::string GetTimezone() {
#if FIREBASE_PLATFORM_WINDOWS
  static bool tz_was_set = false;
  if (!tz_was_set) {
    _tzset();  // Set the time zone used by the below functions, based on OS
               // settings or the TZ variable, as appropriate.
    tz_was_set = true;
  }

  // Get the non-daylight time zone, as the IANA conversion below requires the
  // name of the standard time zone. For example, "Central European Standard
  // Time" which converts to "Europe/Warsaw" or similar.
  std::wstring windows_tz_utf16 = GetWindowsTimezoneInEnglish(0);
  std::string windows_tz_utf8;
  {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> to_utf8;
    windows_tz_utf8 = to_utf8.to_bytes(windows_tz_utf16);
  }

  std::string locale_name = GetLocale();
  wchar_t iana_time_zone_buffer[128];
  bool got_time_zone = false;
  if (locale_name.size() >= 5) {
    wcscpy(iana_time_zone_buffer, L"");
    UErrorCode error_code = (UErrorCode)0;
    int32_t size = 0;
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
      LogDebug(
          "Couldn't convert Windows time zone '%s' with region '%s' to IANA: "
          "%s (%x). Falling back to non-region time zone conversion.",
          windows_tz_utf8.c_str(), region_code.c_str(), u_errorName(error_code),
          error_code);
    }
  }
  if (!got_time_zone) {
    wcscpy(iana_time_zone_buffer, L"");
    UErrorCode error_code = (UErrorCode)0;
    int32_t size = 0;
    // Try without specifying a region
    size = ucal_getTimeZoneIDForWindowsID(
        windows_tz_utf16.c_str(), -1, nullptr, iana_time_zone_buffer,
        sizeof(iana_time_zone_buffer) / sizeof(iana_time_zone_buffer[0]),
        &error_code);
    got_time_zone = (U_SUCCESS(error_code) && size > 0);
    if (!got_time_zone) {
      // Couldn't convert to IANA
      LogDebug(
          "Couldn't convert time zone '%s' to IANA: %s (%x). Falling back to "
          "Windows time zone name.",
          windows_tz_utf8.c_str(), u_errorName(error_code), error_code);
    }
  }
  if (got_time_zone) {
    // One of the above two succeeded, convert the new time zone name back to
    // UTF-8.
    std::wstring iana_tz_utf16(iana_time_zone_buffer);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> to_utf8;
    std::string iana_tz_utf8;
    try {
      iana_tz_utf8 = to_utf8.to_bytes(iana_tz_utf16);
    } catch (std::range_error& ex) {
      LogError("Failed to convert IANA time zone to UTF-8: %s", ex.what());
      got_time_zone = false;
    }
    if (got_time_zone) {
      return iana_tz_utf8;
    }
  }
  if (!got_time_zone) {
    // Either the IANA time zone couldn't be determined, or couldn't be
    // converted into UTF-8 for some reason (the std::range_error above).
    //
    // In any case, return the Windows time zone ID as a backup. We now need to
    // get the correct daylight saving time setting to get the right name.
    //
    // Also, note as above that _get_tzname() doesn't return a UTF-8 name,
    // rather CP-1252, so convert it to UTF-8 (via UTF-16) before returning.

    int daylight = 0;  // daylight savings time?
    if (_get_daylight(&daylight) != 0) {
      // Couldn't determine daylight saving time, return the old name.
      return windows_tz_utf8;
    }
    if (daylight) {
      windows_tz_utf16 = GetWindowsTimezoneInEnglish(daylight);
      {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> to_utf8;
        windows_tz_utf8 = to_utf8.to_bytes(windows_tz_utf16);
      }
    }
    return windows_tz_utf8;
  }

#elif FIREBASE_PLATFORM_LINUX
  // Ubuntu: Check /etc/timezone for the full time zone name.
  FILE* tz_file = fopen("/etc/timezone", "r");
  if (tz_file) {
    const size_t kBufSize = 128;
    char buf[kBufSize];
    if (fgets(buf, kBufSize, tz_file)) {
      // Remove a trailing '\n', if any.
      size_t len = strlen(buf);
      if (buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
      }
      return std::string(buf);
    }
  }
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
