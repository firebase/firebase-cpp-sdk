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

#include "app/rest/curl/curl_global.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "curl/curl.h"

namespace firebase {
namespace rest {
namespace curl {

namespace {

// Intentionally leaked so it is usable during static destruction, matching the
// other process-wide mutexes in app/rest.
Mutex* g_global_mutex = new Mutex();
int g_global_ref_count = 0;

}  // namespace

void CurlGlobal::Acquire() {
  MutexLock lock(*g_global_mutex);
  if (g_global_ref_count == 0) {
    CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
    FIREBASE_ASSERT_MESSAGE(code == CURLE_OK,
                            "curl_global_init failed with code %d",
                            static_cast<int>(code));
  }
  ++g_global_ref_count;
}

void CurlGlobal::Release() {
  MutexLock lock(*g_global_mutex);
  FIREBASE_ASSERT_MESSAGE(g_global_ref_count > 0,
                          "CurlGlobal::Release() without matching Acquire()");
  if (--g_global_ref_count == 0) {
    curl_global_cleanup();
  }
}

int CurlGlobal::ref_count() {
  MutexLock lock(*g_global_mutex);
  return g_global_ref_count;
}

}  // namespace curl
}  // namespace rest
}  // namespace firebase
