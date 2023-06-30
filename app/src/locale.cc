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

// Conversion table from Windows CP-1252 (aka ISO-8859-1) 1-byte
// encoding to UTF-16. 0xFFFD (Unicode replacement character) is used
// for some values that have no UTF-16 equivalent.
static wchar_t lookup_cp1252_to_utf16[256] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
    0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F, 0x0010, 0x0011,
    0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001A,
    0x001B, 0x001C, 0x001D, 0x001E, 0x001F, 0x0020, 0x0021, 0x0022, 0x0023,
    0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C,
    0x002D, 0x002E, 0x002F, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035,
    0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E,
    0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050,
    0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
    0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F, 0x0060, 0x0061, 0x0062,
    0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
    0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D,
    0x007E, 0x007F, 0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020,
    0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC,
    0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178, 0x00A0, 0x00A1,
    0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA,
    0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF, 0x00B0, 0x00B1, 0x00B2, 0x00B3,
    0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC,
    0x00BD, 0x00BE, 0x00BF, 0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5,
    0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE,
    0x00CF, 0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF, 0x00E0,
    0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9,
    0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF, 0x00F0, 0x00F1, 0x00F2,
    0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB,
    0x00FC, 0x00FD, 0x00FE, 0x00FF};

std::wstring convert_cp1252_to_utf16(const char* str_cp1252) {
  size_t len = strlen(str_cp1252);
  std::vector<wchar_t> buf_utf16(len + 1);
  const unsigned char* unsigned_str_cp1252 =
      reinterpret_cast<const unsigned char*>(str_cp1252);
  for (int i = 0; i < len + 1; i++) {
    buf_utf16[i] = lookup_cp1252_to_utf16[unsigned_str_cp1252[i]];
  }
  return std::wstring(&buf_utf16[0]);
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
  // Get the standard time zone name.
  std::string windows_tz_cp1252;
  {
    size_t length = 0;  // get the needed string length
    if (_get_tzname(&length, nullptr, 0, 0) != 0) return "";
    std::vector<char> namebuf(length);
    if (_get_tzname(&length, &namebuf[0], length, 0) != 0) return "";
    windows_tz_cp1252 = std::string(&namebuf[0]);
  }

  // Convert time zone name to wide string, then into UTF-8.
  std::wstring windows_tz_utf16 =
      convert_cp1252_to_utf16(windows_tz_cp1252.c_str());
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
      // Daylight saving time is active, get a new tzname and convert to UTF-8.
      size_t length = 0;  // get the needed string length
      if (_get_tzname(&length, nullptr, 0, 1) != 0) {
        // Couldn't get tzname length, return the old name.
        return windows_tz_utf8;
      }
      std::vector<char> namebuf(length);
      if (_get_tzname(&length, &namebuf[0], length, 1) != 0) {
        // Couldn't get tzname, return the old name.
        return windows_tz_utf8;
      }
      windows_tz_cp1252 = std::string(&namebuf[0]);
      // Convert time zone name to wide string, then into UTF-8.
      windows_tz_utf16 = convert_cp1252_to_utf16(windows_tz_cp1252.c_str());
      {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> to_utf8;
        windows_tz_utf8 = to_utf8.to_bytes(windows_tz_utf16);
      }
    }
    return windows_tz_utf8;
  }

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
