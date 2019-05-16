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

#include "app/rest/util.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <string>

#include "app/src/variant_util.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "curl/curl.h"

namespace firebase {
namespace rest {
namespace util {

const char kHttpHeaderSeparator = ':';
const char kAccept[] = "Accept";
const char kAuthorization[] = "Authorization";
const char kContentType[] = "Content-Type";
const char kApplicationJson[] = "application/json";
const char kApplicationWwwFormUrlencoded[] =
    "application/x-www-form-urlencoded";
const char kDate[] = "Date";
const char kCrLf[] = "\r\n";
const char kGet[] = "GET";
const char kPost[] = "POST";

// Mutex for utils initialization and distribution of CURL pointers.
static ::firebase::Mutex g_util_curl_mutex;  // NOLINT

CURL* g_curl_instance = nullptr;
int g_init_ref_count = 0;

void* CreateCurlPtr() {  // NOLINT
  MutexLock lock(g_util_curl_mutex);
  return curl_easy_init();
}

void DestroyCurlPtr(void* curl_ptr) {
  MutexLock lock(g_util_curl_mutex);
  curl_easy_cleanup(static_cast<CURL*>(curl_ptr));
}

void Initialize() {
  MutexLock curl_lock(g_util_curl_mutex);
  assert(g_init_ref_count >= 0);
  if (g_init_ref_count == 0) {
    g_curl_instance = reinterpret_cast<CURL*>(CreateCurlPtr());
  }
  ++g_init_ref_count;
}

void Terminate() {
  MutexLock curl_lock(g_util_curl_mutex);
  --g_init_ref_count;
  assert(g_init_ref_count >= 0);
  if (g_init_ref_count == 0) {
    assert(g_curl_instance != nullptr);
    DestroyCurlPtr(g_curl_instance);
    g_curl_instance = nullptr;
  }
}

std::string EncodeUrl(const std::string& path) {
  assert(g_curl_instance != nullptr);
  char* encoded_string =
      curl_easy_escape(g_curl_instance, path.c_str(), path.length());
  std::string result(encoded_string);
  curl_free(encoded_string);
  return result;
}

std::string DecodeUrl(const std::string& path) {
  assert(g_curl_instance != nullptr);
  int length = 0;
  char* decoded_string =
      curl_easy_unescape(g_curl_instance, path.c_str(), path.length(), &length);
  std::string result(decoded_string, length);
  curl_free(decoded_string);
  return result;
}

std::string TrimWhitespace(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\n\v\f\r");
  // In case str contains only whitespace, return an empty string.
  if (start == std::string::npos) {
    return "";
  }
  // Since now str contains at least a non-whitespace, end cannot be npos.
  size_t end = str.find_last_not_of(" \t\n\v\f\r");
  return str.substr(start, end - start + 1);
}

std::string ToUpper(const std::string& str) {
  std::string res = str;
  std::transform(res.begin(), res.end(), res.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return res;
}

bool JsonData::Parse(const char* json_txt) {
  root_ = firebase::util::JsonToVariant(json_txt);
  return !root_.is_null();
}

}  // namespace util
}  // namespace rest
}  // namespace firebase
