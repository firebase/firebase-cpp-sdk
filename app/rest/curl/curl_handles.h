/*
 * Copyright 2026 Google LLC
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

#ifndef FIREBASE_APP_REST_CURL_CURL_HANDLES_H_
#define FIREBASE_APP_REST_CURL_CURL_HANDLES_H_

#include <memory>
#include <type_traits>

#include "curl/curl.h"

namespace firebase {
namespace rest {
namespace curl {

// RAII ownership for the three libcurl handle types. All deleters are
// null-safe. These are plain std::unique_ptr aliases so ownership transfer,
// release() and reset() behave exactly as for any other unique_ptr.
//
// They are written against CURL_STRICTER (which the rest library compiles
// with), i.e. CURL and CURLM are distinct incomplete struct types rather than
// void.

struct EasyDeleter {
  void operator()(CURL* easy) const {
    if (easy != nullptr) curl_easy_cleanup(easy);
  }
};

struct MultiDeleter {
  void operator()(CURLM* multi) const {
    if (multi != nullptr) curl_multi_cleanup(multi);
  }
};

struct SListDeleter {
  void operator()(curl_slist* list) const {
    if (list != nullptr) curl_slist_free_all(list);
  }
};

using UniqueEasy = std::unique_ptr<CURL, EasyDeleter>;
using UniqueMulti = std::unique_ptr<CURLM, MultiDeleter>;
using UniqueSList = std::unique_ptr<curl_slist, SListDeleter>;

// Appends a line to an owned curl_slist, replacing the owned pointer.
// curl_slist_append() returns nullptr on allocation failure, in which case the
// existing list is left untouched and false is returned.
inline bool SListAppend(UniqueSList* list, const char* line) {
  curl_slist* appended = curl_slist_append(list->get(), line);
  if (appended == nullptr) return false;
  (void)list->release();  // appended now owns the (possibly same) head node.
  list->reset(appended);
  return true;
}

// Typed wrapper around the variadic curl_easy_setopt().
//
// curl_easy_setopt() is a C variadic function, so passing e.g. an `int` where
// libcurl expects a `long` is undefined behavior that compiles silently. This
// template only accepts the argument types libcurl actually reads from the
// va_list: `long`, `curl_off_t`, and pointers (object pointers, C strings,
// curl_slist*, and callback function pointers). Anything else - including
// `int`, `bool`, enums and a bare `nullptr` - is rejected at compile time;
// write `1L` or cast explicitly (e.g. `static_cast<char*>(nullptr)`).
template <typename T>
inline CURLcode SetOpt(CURL* easy, CURLoption option, T value) {
  static_assert(std::is_pointer<T>::value || std::is_same<T, long>::value ||
                    std::is_same<T, curl_off_t>::value,
                "curl_easy_setopt() only accepts long, curl_off_t or pointer "
                "arguments; cast the value explicitly");
  return curl_easy_setopt(easy, option, value);
}

}  // namespace curl
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CURL_CURL_HANDLES_H_
