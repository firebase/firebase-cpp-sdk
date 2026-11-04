#ifndef FIREBASE_REMOTE_CONFIG_SRC_VALUE_PARSER_H_
#define FIREBASE_REMOTE_CONFIG_SRC_VALUE_PARSER_H_
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include "app/src/include/firebase/variant.h"
namespace firebase {
namespace remote_config {
namespace internal {
class ValueParser {
 public:
  static bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
      char ca = a[i];
      char cb = b[i];
      if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
      if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
      if (ca != cb) return false;
    }
    return true;
  }
  static bool IsBoolTrue(std::string_view str) {
    return str == "1" || EqualsIgnoreCase(str, "true") ||
           EqualsIgnoreCase(str, "t") || EqualsIgnoreCase(str, "yes") ||
           EqualsIgnoreCase(str, "y") || EqualsIgnoreCase(str, "on");
  }
  static bool IsBoolFalse(std::string_view str) {
    return str.empty() || str == "0" || EqualsIgnoreCase(str, "false") ||
           EqualsIgnoreCase(str, "f") || EqualsIgnoreCase(str, "no") ||
           EqualsIgnoreCase(str, "n") || EqualsIgnoreCase(str, "off");
  }
  static bool ParseBoolean(std::string_view str, bool* out) {
    if (IsBoolTrue(str)) {
      if (out) *out = true;
      return true;
    }
    if (IsBoolFalse(str)) {
      if (out) *out = false;
      return true;
    }
    if (out) *out = false;
    return false;
  }
  static bool ParseInt64(std::string_view str, int64_t* out) {
    if (str.empty()) {
      if (out) *out = 0;
      return false;
    }
    size_t i = 0;
    bool negative = false;
    if (str[0] == '+' || str[0] == '-') {
      negative = (str[0] == '-');
      ++i;
    }
    if (i >= str.size()) {
      if (out) *out = 0;
      return false;
    }
    int base = 10;
    if (i + 2 <= str.size() && str[i] == '0' &&
        (str[i + 1] == 'x' || str[i + 1] == 'X')) {
      base = 16;
      i += 2;
      if (i >= str.size()) {
        if (out) *out = 0;
        return false;
      }
    }
    for (size_t j = i; j < str.size(); ++j) {
      char c = str[j];
      bool ok = (c >= '0' && c <= '9') ||
                (base == 16 &&
                 ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));
      if (!ok) {
        if (out) *out = 0;
        return false;
      }
    }
    std::string clean;
    if (negative) {
      clean.push_back('-');
    }
    clean.append(str.data() + i, str.size() - i);
    int64_t val = 0;
    const char* begin = clean.data();
    const char* end = begin + clean.size();
    auto [ptr, ec] = std::from_chars(begin, end, val, base);
    if (ec != std::errc() || ptr != end) {
      if (out) *out = 0;
      return false;
    }
    if (out) *out = val;
    return true;
  }
  static bool IsLong(std::string_view str) {
    return ParseInt64(str, nullptr);
  }
  static bool IsDoubleSyntax(std::string_view str) {
    if (str.empty()) return false;
    size_t i = 0;
    if (str[i] == '+' || str[i] == '-') {
      ++i;
    }
    if (i >= str.size()) return false;
    bool has_digits = false;
    while (i < str.size() && str[i] >= '0' && str[i] <= '9') {
      has_digits = true;
      ++i;
    }
    if (i < str.size() && str[i] == '.') {
      ++i;
      while (i < str.size() && str[i] >= '0' && str[i] <= '9') {
        has_digits = true;
        ++i;
      }
    }
    if (!has_digits) return false;
    if (i < str.size() && (str[i] == 'e' || str[i] == 'E')) {
      ++i;
      if (i < str.size() && (str[i] == '+' || str[i] == '-')) {
        ++i;
      }
      if (i >= str.size()) return false;
      while (i < str.size() && str[i] >= '0' && str[i] <= '9') {
        ++i;
      }
    }
    return i == str.size();
  }
  static bool ParseDouble(std::string_view str, double* out) {
    if (!IsDoubleSyntax(str)) {
      if (out) *out = 0.0;
      return false;
    }
    std::istringstream iss{std::string(str)};
    iss.imbue(std::locale::classic());
    double val = 0.0;
    iss >> val;
    if (iss.fail() || !std::isfinite(val)) {
      if (out) *out = 0.0;
      return false;
    }
    if (out) *out = val;
    return true;
  }
  static bool IsDouble(std::string_view str) {
    return ParseDouble(str, nullptr);
  }
  static Variant CoerceToVariant(std::string_view str) {
    if (str.empty()) {
      return Variant::FromMutableString("");
    }
    int64_t lval = 0;
    if (ParseInt64(str, &lval)) {
      return Variant::FromInt64(lval);
    }
    double dval = 0.0;
    if (ParseDouble(str, &dval)) {
      return Variant::FromDouble(dval);
    }
    bool bval = false;
    if (ParseBoolean(str, &bval)) {
      return Variant::FromBool(bval);
    }
    return Variant::FromMutableString(std::string(str));
  }
  static std::string FormatFixedDouble(double val) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << std::fixed << std::setprecision(16) << val;
    return ss.str();
  }
  static std::string FormatGeneralDouble(double val) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << std::setprecision(15) << val;
    return ss.str();
  }
};
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
#endif
