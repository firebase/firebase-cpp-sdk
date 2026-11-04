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

#include "curl/curl.h"

// The desktop SDK drives libcurl through APIs that only exist in recent
// releases: curl_multi_poll()/curl_multi_wakeup() (7.68), CURLOPT_PROTOCOLS_STR
// (7.85), CURLOPT_CONNECT_ONLY=2 WebSocket handshakes (7.86) and
// CURLWS_NOAUTOPONG (8.14). Fail at compile time with a clear message rather
// than with scattered "undeclared identifier" errors when a user points
// CURL_SOURCE_DIR at an older tree.
static_assert(LIBCURL_VERSION_NUM >= 0x080e00,
              "Firebase C++ SDK requires libcurl >= 8.14.0 (WebSockets with "
              "CURLWS_NOAUTOPONG)");

namespace firebase {
namespace rest {
namespace curl {

// RAII ownership for libcurl easy, multi, and slist handles. All deleters are
// null-safe. These are plain std::unique_ptr aliases so ownership transfer,
// release() and reset() behave exactly as for any other unique_ptr.
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

// Reference-counted curl_global_init() / curl_global_cleanup(). Thread-safe.
// Every libcurl user in the SDK holds a reference while it owns any CURL
// handle, so libcurl's process-wide state is initialized exactly once and torn
// down only after the last user is gone.
void AcquireCurlGlobal();
void ReleaseCurlGlobal();

}  // namespace curl
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CURL_CURL_HANDLES_H_
