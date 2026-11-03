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

#include <cstdint>
#include <cstring>

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
#include <fstream>
#else
#error "Unknown platform."
#endif  // platform selector

#include <algorithm>
#include <cstdlib>
#include <locale>
#include <string>
#include <vector>

#include "app/src/thread.h"

namespace firebase {
namespace internal {

#if FIREBASE_PLATFORM_WINDOWS
namespace {

std::string WideToUtf8(const std::wstring& input, bool* out_success = nullptr) {
  if (out_success) {
    *out_success = true;
  }
  if (input.empty()) {
    return "";
  }
  int utf8_len = ::WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, input.data(),
      static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
  if (utf8_len <= 0) {
    if (out_success) {
      *out_success = false;
    }
    return "";
  }
  std::string output(static_cast<size_t>(utf8_len), '\0');
  int converted_len = ::WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, input.data(),
      static_cast<int>(input.size()), &output[0], utf8_len, nullptr, nullptr);
  if (converted_len != utf8_len) {
    if (out_success) {
      *out_success = false;
    }
    return "";
  }
  return output;
}

}  // namespace
#endif  // FIREBASE_PLATFORM_WINDOWS

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
  std::string output = WideToUtf8(woutput);
  // Change all hyphens to underscores to normalize the locale.
  std::replace(output.begin(), output.end(), '-', '_');
  return output;
#elif FIREBASE_PLATFORM_LINUX
  // If std::locale() has been customized, return it, else return the contents
  // of the LANG or LC_CTYPE environment variables if set, or otherwise return a
  // default locale (empty in real life, or placeholder when running in a unit
  // test, as the test environment has no locale variables set).
  std::string output;
  std::string current_locale = std::locale().name();
  if (current_locale != "C" && !current_locale.empty()) {
    output = current_locale;
  } else {
    const char* lang_env = std::getenv("LANG");
    const char* lc_ctype_env = std::getenv("LC_CTYPE");
    const char* test_tmpdir_env = std::getenv("TEST_TMPDIR");
    if (lang_env && *lang_env) {
      output = lang_env;
    } else if (lc_ctype_env && *lc_ctype_env) {
      output = lc_ctype_env;
    } else if (test_tmpdir_env && *test_tmpdir_env) {
      output = "en_US";
    }
  }
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
  // Set the time zone used by the below functions, based on OS settings or the
  // TZ variable, as appropriate.
  static const bool tz_was_set = (_tzset(), true);
  (void)tz_was_set;

  // Get the non-daylight time zone, as the IANA conversion below requires the
  // name of the standard time zone. For example, "Central European Standard
  // Time" which converts to "Europe/Warsaw" or similar.
  std::wstring windows_tz_utf16 = GetWindowsTimezoneInEnglish(0);
  std::string windows_tz_utf8 = WideToUtf8(windows_tz_utf16);

  std::string locale_name = GetLocale();
  constexpr size_t kIanaBufferCapacity = 128;
  wchar_t iana_time_zone_buffer[kIanaBufferCapacity];
  int32_t iana_tz_size = 0;
  bool got_time_zone = false;
  size_t sep_pos = locale_name.find_last_of("_-");
  if (sep_pos != std::string::npos && sep_pos + 1 < locale_name.size()) {
    iana_time_zone_buffer[0] = L'\0';
    UErrorCode error_code = U_ZERO_ERROR;
    // Try time zone first with the region code returned above.
    // For example, "en_US" -> "US", "fil_PH" -> "PH", "zh_Hans_CN" -> "CN".
    std::string region_code = locale_name.substr(sep_pos + 1);
    iana_tz_size = ucal_getTimeZoneIDForWindowsID(
        windows_tz_utf16.c_str(), -1, region_code.c_str(),
        iana_time_zone_buffer, static_cast<int32_t>(kIanaBufferCapacity),
        &error_code);
    got_time_zone =
        (U_SUCCESS(error_code) && iana_tz_size > 0 &&
         static_cast<size_t>(iana_tz_size) <= kIanaBufferCapacity);
    if (!got_time_zone) {
      LogDebug(
          "Couldn't convert Windows time zone '%s' with region '%s' to IANA: "
          "%s (%x). Falling back to non-region time zone conversion.",
          windows_tz_utf8.c_str(), region_code.c_str(), u_errorName(error_code),
          error_code);
    }
  }
  if (!got_time_zone) {
    iana_time_zone_buffer[0] = L'\0';
    UErrorCode error_code = U_ZERO_ERROR;
    // Try without specifying a region
    iana_tz_size = ucal_getTimeZoneIDForWindowsID(
        windows_tz_utf16.c_str(), -1, nullptr, iana_time_zone_buffer,
        static_cast<int32_t>(kIanaBufferCapacity), &error_code);
    got_time_zone =
        (U_SUCCESS(error_code) && iana_tz_size > 0 &&
         static_cast<size_t>(iana_tz_size) <= kIanaBufferCapacity);
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
    std::wstring iana_tz_utf16(iana_time_zone_buffer,
                               static_cast<size_t>(iana_tz_size));
    bool converted = false;
    std::string iana_tz_utf8 = WideToUtf8(iana_tz_utf16, &converted);
    if (!converted) {
      LogError("Failed to convert IANA time zone to UTF-8");
      got_time_zone = false;
    } else {
      return iana_tz_utf8;
    }
  }
  if (!got_time_zone) {
    // Either the IANA time zone couldn't be determined, or couldn't be
    // converted into UTF-8 for some reason.
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
      windows_tz_utf8 = WideToUtf8(windows_tz_utf16);
    }
    return windows_tz_utf8;
  }

#elif FIREBASE_PLATFORM_LINUX
  // Ubuntu: Check /etc/timezone for the full time zone name.
  std::ifstream tz_file("/etc/timezone");
  std::string tz;
  if (std::getline(tz_file, tz)) {
    // Trim trailing whitespace, including the '\r' of a CRLF line ending.
    size_t last_non_ws = tz.find_last_not_of(" \t\r\n");
    if (last_non_ws != std::string::npos) {
      tz.erase(last_non_ws + 1);
      if (!tz.empty()) {
        return tz;
      }
    }
  }
  // If TZ environment variable is defined and not empty, use it, else use
  // localtime_r / tzname.
  const char* tz_env = std::getenv("TZ");
  if (tz_env && *tz_env) {
    return tz_env;
  }
  tzset();
  std::time_t now = std::time(nullptr);
  std::tm local_tm{};
  if (localtime_r(&now, &local_tm) != nullptr &&
      local_tm.tm_zone != nullptr && local_tm.tm_zone[0] != '\0') {
    return local_tm.tm_zone;
  }
  int is_dst = (local_tm.tm_isdst > 0) ? 1 : (daylight ? 1 : 0);
  const char* tz_str = tzname[is_dst];
  return tz_str ? tz_str : "";
#else
  // An error was already triggered above.
  return "";
#endif  // platform selector
}

}  // namespace internal
}  // namespace firebase
