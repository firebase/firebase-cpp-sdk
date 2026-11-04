// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_APP_SRC_JNI_STRING_H_
#define FIREBASE_APP_SRC_JNI_STRING_H_

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "app/src/jni/local.h"
#include "app/src/jni/ownership.h"

namespace firebase::jni {

class String : public Object {
 public:
  using JniType = jstring;
  constexpr String() noexcept = default;
  explicit constexpr String(jstring string) noexcept : Object(string) {}
  constexpr jstring get() const noexcept {
    return static_cast<jstring>(object_);
  }
  std::string ToString(Env& env) const;
};

inline constexpr bool IsValidModifiedUtf8(std::string_view utf8) noexcept {
  size_t i = 0;
  const size_t n = utf8.size();
  while (i < n) {
    const uint8_t b0 = static_cast<uint8_t>(utf8[i]);
    if (b0 == 0x00) {
      return false;
    } else if (b0 < 0x80) {
      ++i;
    } else if (b0 >= 0xC2 && b0 <= 0xDF) {
      if (i + 1 >= n || (static_cast<uint8_t>(utf8[i + 1]) & 0xC0) != 0x80)
        return false;
      i += 2;
    } else if (b0 == 0xC0) {
      if (i + 1 >= n || static_cast<uint8_t>(utf8[i + 1]) != 0x80) return false;
      i += 2;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
      if (i + 2 >= n) return false;
      const uint8_t b1 = static_cast<uint8_t>(utf8[i + 1]);
      const uint8_t b2 = static_cast<uint8_t>(utf8[i + 2]);
      if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 ||
          (b0 == 0xE0 && b1 < 0xA0)) {
        return false;
      }
      i += 3;
    } else {
      return false;
    }
  }
  return true;
}

inline std::string Utf8ToModifiedUtf8(std::string_view utf8) {
  std::string out;
  out.reserve(utf8.size() + 4);
  size_t i = 0;
  const size_t n = utf8.size();
  while (i < n) {
    const uint8_t b0 = static_cast<uint8_t>(utf8[i]);
    if (b0 == 0x00) {
      out.append("\xC0\x80", 2);
      ++i;
    } else if (b0 < 0x80) {
      out.push_back(static_cast<char>(b0));
      ++i;
    } else if (b0 == 0xC0 && i + 1 < n &&
               static_cast<uint8_t>(utf8[i + 1]) == 0x80) {
      out.append("\xC0\x80", 2);
      i += 2;
    } else if (b0 >= 0xC2 && b0 <= 0xDF && i + 1 < n &&
               (static_cast<uint8_t>(utf8[i + 1]) & 0xC0) == 0x80) {
      out.append(utf8.data() + i, 2);
      i += 2;
    } else if (b0 >= 0xE0 && b0 <= 0xEF && i + 2 < n) {
      const uint8_t b1 = static_cast<uint8_t>(utf8[i + 1]);
      const uint8_t b2 = static_cast<uint8_t>(utf8[i + 2]);
      if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 &&
          !(b0 == 0xE0 && b1 < 0xA0)) {
        out.append(utf8.data() + i, 3);
        i += 3;
      } else {
        out.append("\xEF\xBF\xBD", 3);
        ++i;
      }
    } else if (b0 >= 0xF0 && b0 <= 0xF4 && i + 3 < n) {
      const uint8_t b1 = static_cast<uint8_t>(utf8[i + 1]);
      const uint8_t b2 = static_cast<uint8_t>(utf8[i + 2]);
      const uint8_t b3 = static_cast<uint8_t>(utf8[i + 3]);
      if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
        const uint32_t cp = (static_cast<uint32_t>(b0 & 0x07) << 18) |
                            (static_cast<uint32_t>(b1 & 0x3F) << 12) |
                            (static_cast<uint32_t>(b2 & 0x3F) << 6) |
                            static_cast<uint32_t>(b3 & 0x3F);
        if (cp >= 0x10000 && cp <= 0x10FFFF) {
          const uint32_t u = cp - 0x10000;
          const uint16_t high = static_cast<uint16_t>(0xD800 + (u >> 10));
          const uint16_t low = static_cast<uint16_t>(0xDC00 + (u & 0x3FF));
          char buf[6] = {
              static_cast<char>(0xE0 | (high >> 12)),
              static_cast<char>(0x80 | ((high >> 6) & 0x3F)),
              static_cast<char>(0x80 | (high & 0x3F)),
              static_cast<char>(0xE0 | (low >> 12)),
              static_cast<char>(0x80 | ((low >> 6) & 0x3F)),
              static_cast<char>(0x80 | (low & 0x3F)),
          };
          out.append(buf, 6);
        } else {
          out.append("\xEF\xBF\xBD", 3);
        }
        i += 4;
      } else {
        out.append("\xEF\xBF\xBD", 3);
        ++i;
      }
    } else {
      out.append("\xEF\xBF\xBD", 3);
      ++i;
    }
  }
  return out;
}

inline std::string ModifiedUtf8ToUtf8(std::string_view mutf8) {
  bool needs_decoding = false;
  for (size_t i = 0; i < mutf8.size(); ++i) {
    const uint8_t b = static_cast<uint8_t>(mutf8[i]);
    if (b == 0xC0 || (b == 0xED && (i + 1 >= mutf8.size() ||
                                    (static_cast<uint8_t>(mutf8[i + 1]) >= 0xA0 &&
                                     static_cast<uint8_t>(mutf8[i + 1]) <= 0xBF)))) {
      needs_decoding = true;
      break;
    }
  }
  if (!needs_decoding) return std::string(mutf8);

  std::string out;
  out.reserve(mutf8.size());
  size_t i = 0;
  const size_t n = mutf8.size();
  while (i < n) {
    const uint8_t b0 = static_cast<uint8_t>(mutf8[i]);
    if (b0 == 0xC0 && i + 1 < n &&
        static_cast<uint8_t>(mutf8[i + 1]) == 0x80) {
      out.push_back('\0');
      i += 2;
    } else if (b0 == 0xED) {
      if (i + 5 < n) {
        const uint8_t b1 = static_cast<uint8_t>(mutf8[i + 1]);
        const uint8_t b2 = static_cast<uint8_t>(mutf8[i + 2]);
        const uint8_t b3 = static_cast<uint8_t>(mutf8[i + 3]);
        const uint8_t b4 = static_cast<uint8_t>(mutf8[i + 4]);
        const uint8_t b5 = static_cast<uint8_t>(mutf8[i + 5]);
        if (b1 >= 0xA0 && b1 <= 0xAF && (b2 & 0xC0) == 0x80 && b3 == 0xED &&
            b4 >= 0xB0 && b4 <= 0xBF && (b5 & 0xC0) == 0x80) {
          const uint32_t high =
              0xD800U | (static_cast<uint32_t>(b1 & 0x0F) << 6) |
              static_cast<uint32_t>(b2 & 0x3F);
          const uint32_t low =
              0xDC00U | (static_cast<uint32_t>(b4 & 0x0F) << 6) |
              static_cast<uint32_t>(b5 & 0x3F);
          const uint32_t cp =
              0x10000U + (((high - 0xD800U) << 10) | (low - 0xDC00U));
          char buf[4] = {
              static_cast<char>(0xF0 | (cp >> 18)),
              static_cast<char>(0x80 | ((cp >> 12) & 0x3F)),
              static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
              static_cast<char>(0x80 | (cp & 0x3F)),
          };
          out.append(buf, 4);
          i += 6;
          continue;
        }
      }
      if (i + 2 < n && static_cast<uint8_t>(mutf8[i + 1]) >= 0xA0 &&
          static_cast<uint8_t>(mutf8[i + 1]) <= 0xBF &&
          (static_cast<uint8_t>(mutf8[i + 2]) & 0xC0) == 0x80) {
        out.append("\xEF\xBF\xBD");
        i += 3;
      } else if (i + 1 < n && static_cast<uint8_t>(mutf8[i + 1]) >= 0xA0 &&
                 static_cast<uint8_t>(mutf8[i + 1]) <= 0xBF) {
        out.append("\xEF\xBF\xBD");
        i += 2;
      } else if (i + 1 == n) {
        out.append("\xEF\xBF\xBD");
        ++i;
      } else {
        out.push_back(static_cast<char>(b0));
        ++i;
      }
    } else {
      out.push_back(static_cast<char>(b0));
      ++i;
    }
  }
  return out;
}

inline std::string ModifiedUtf8ToUtf8(const char* data, size_t len) {
  return (!data || len == 0) ? std::string()
                             : ModifiedUtf8ToUtf8(std::string_view(data, len));
}

inline jstring NewRawJString(JNIEnv* env, const char* utf8) {
  if (!env || !utf8 || env->ExceptionCheck()) return nullptr;
  std::string_view sv(utf8);
  if (IsValidModifiedUtf8(sv)) return env->NewStringUTF(utf8);
  std::string mutf8 = Utf8ToModifiedUtf8(sv);
  return env->NewStringUTF(mutf8.c_str());
}

inline jstring NewRawJString(JNIEnv* env, const std::string& utf8) {
  if (!env || env->ExceptionCheck()) return nullptr;
  if (IsValidModifiedUtf8(utf8)) return env->NewStringUTF(utf8.c_str());
  std::string mutf8 = Utf8ToModifiedUtf8(utf8);
  return env->NewStringUTF(mutf8.c_str());
}

inline jstring NewRawJString(JNIEnv* env, std::string_view utf8) {
  if (!env || env->ExceptionCheck()) return nullptr;
  std::string mutf8 = Utf8ToModifiedUtf8(utf8);
  return env->NewStringUTF(mutf8.c_str());
}

inline ScopedLocalRef<jstring> NewJString(JNIEnv* env, const char* utf8) {
  return ScopedLocalRef<jstring>(env, NewRawJString(env, utf8), true);
}
inline ScopedLocalRef<jstring> NewJString(JNIEnv* env,
                                          const std::string& utf8) {
  return ScopedLocalRef<jstring>(env, NewRawJString(env, utf8), true);
}
inline ScopedLocalRef<jstring> NewJString(JNIEnv* env, std::string_view utf8) {
  return ScopedLocalRef<jstring>(env, NewRawJString(env, utf8), true);
}

inline std::string JStringToString(JNIEnv* env, jobject str_obj) {
  if (!env || !str_obj || env->ExceptionCheck()) return std::string();
  jstring str = static_cast<jstring>(str_obj);
  const char* chars = env->GetStringUTFChars(str, nullptr);
  if (!chars) return std::string();
  jsize len = env->GetStringUTFLength(str);
  std::string result =
      ModifiedUtf8ToUtf8(std::string_view(chars, static_cast<size_t>(len)));
  env->ReleaseStringUTFChars(str, chars);
  return result;
}

inline std::string JniStringToStringAndReleaseLocalRef(JNIEnv* env,
                                                       jobject str_obj) {
  if (!env || !str_obj) return std::string();
  std::string result = JStringToString(env, str_obj);
  env->DeleteLocalRef(str_obj);
  return result;
}

}  // namespace firebase::jni

#endif  // FIREBASE_APP_SRC_JNI_STRING_H_
