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

#ifndef FIREBASE_APP_REST_CURL_CURL_GLOBAL_H_
#define FIREBASE_APP_REST_CURL_CURL_GLOBAL_H_

namespace firebase {
namespace rest {
namespace curl {

// Reference-counted wrapper around curl_global_init(CURL_GLOBAL_ALL) /
// curl_global_cleanup().
//
// libcurl requires curl_global_init() to be called exactly once before any
// other libcurl function and curl_global_cleanup() once after the last one;
// neither is thread-safe. Several independent SDK products (Auth, Storage,
// Functions, Remote Config, Realtime Database, App Check) may each want
// libcurl alive for their own lifetime, so this class counts acquisitions and
// only performs the real init on 0 -> 1 and the real cleanup on 1 -> 0. Both
// methods are thread-safe with respect to each other.
class CurlGlobal {
 public:
  // Increments the reference count, calling curl_global_init() on the first
  // acquisition. Asserts if libcurl fails to initialize.
  static void Acquire();
  // Decrements the reference count, calling curl_global_cleanup() when it
  // reaches zero. Must be balanced with a prior Acquire().
  static void Release();
  // Current reference count (for tests/diagnostics).
  static int ref_count();
};

// Scoped CurlGlobal reference.
class CurlGlobalRef {
 public:
  CurlGlobalRef() { CurlGlobal::Acquire(); }
  ~CurlGlobalRef() { CurlGlobal::Release(); }
  CurlGlobalRef(const CurlGlobalRef&) = delete;
  CurlGlobalRef& operator=(const CurlGlobalRef&) = delete;
};

}  // namespace curl
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CURL_CURL_GLOBAL_H_
