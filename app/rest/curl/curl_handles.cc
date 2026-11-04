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

#include "app/rest/curl/curl_handles.h"

#include <cstdint>
#include <mutex>  // NOLINT

#include "app/src/assert.h"

namespace firebase {
namespace rest {
namespace curl {

namespace {

std::mutex g_curl_global_mutex;
int64_t g_curl_global_refcount = 0;

}  // namespace

void AcquireCurlGlobal() {
  std::lock_guard<std::mutex> lock(g_curl_global_mutex);
  if (g_curl_global_refcount++ == 0) {
    CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
    FIREBASE_ASSERT_MESSAGE(code == CURLE_OK,
                            "curl_global_init(CURL_GLOBAL_ALL) failed: %s",
                            curl_easy_strerror(code));
  }
}

void ReleaseCurlGlobal() {
  std::lock_guard<std::mutex> lock(g_curl_global_mutex);
  FIREBASE_ASSERT_MESSAGE(
      g_curl_global_refcount > 0,
      "ReleaseCurlGlobal called more times than AcquireCurlGlobal");
  if (--g_curl_global_refcount == 0) {
    curl_global_cleanup();
  }
}

}  // namespace curl
}  // namespace rest
}  // namespace firebase
