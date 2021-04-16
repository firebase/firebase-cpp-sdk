/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_UTIL_H_
#define FIREBASE_APP_CLIENT_CPP_REST_UTIL_H_

#include <string>

#include "app/src/include/firebase/variant.h"
#include "app/src/mutex.h"
#include "flatbuffers/idl.h"

namespace firebase {
namespace rest {
namespace util {

// The separator between field name and value in HTTP headers.
extern const char kHttpHeaderSeparator;
// String literals for a few common header strings (names and values).
extern const char kAccept[];
extern const char kAuthorization[];
extern const char kContentType[];
extern const char kApplicationJson[];
extern const char kApplicationWwwFormUrlencoded[];
extern const char kDate[];
// The CRLF literal.
extern const char kCrLf[];
// String literals for a few common HTTP methods.
extern const char kGet[];
extern const char kPost[];

// HTTP status codes that REST APIs might return.
enum HttpStatusCode {
  HttpInvalid = 0,
  HttpSuccess = 200,
  HttpNoContent = 204,
  HttpBadRequest = 400,
  HttpPaymentRequired = 402,
  HttpUnauthorized = 401,
  HttpForbidden = 403,
  HttpNotFound = 404,
  HttpRequestTimeout = 408
};

// Initialize utilities.  This must be called before any functions that
// depend on libcurl are called.  It's reference counted, so multiple
// modules can use it without trouble.
void Initialize();

// Cleans up utilities.  Call this if you've called Initialize() and are
// exiting.  Reference counted for convenience.
void Terminate();

// Returns a new CURL pointer.   Mutex-locked, for thread safety.
void* CreateCurlPtr();

// Disposes of a CURL pointer.   Mutex-locked, for thread safety.
void DestroyCurlPtr(void* curl_ptr);

// Trim the string's leading and trailing white space.
std::string TrimWhitespace(const std::string& str);

// Change to upper cases.
std::string ToUpper(const std::string& str);

// Apply URL encoding to a string.
std::string EncodeUrl(const std::string& path);

// Decode a URL-encoded string
std::string DecodeUrl(const std::string& path);

// Generic base class for retrieving JSON data and marshalling it via
// flexbuffers.
class JsonData {
 public:
  virtual ~JsonData() {}

  virtual bool Parse(const char* json_txt);
  Variant root() { return root_; }
  virtual bool is_valid() { return !root_.is_null(); }

 protected:
  bool parsed_;
  Variant root_;
};

}  // namespace util
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_UTIL_H_
