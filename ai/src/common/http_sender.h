/*
 * Copyright 2025 Google LLC
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

#ifndef FIREBASE_AI_SRC_COMMON_HTTP_SENDER_H_
#define FIREBASE_AI_SRC_COMMON_HTTP_SENDER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "firebase/app.h"

namespace firebase {
namespace ai {
namespace internal {

/// @brief Platform-agnostic representation of an HTTP request.
struct HttpRequest {
  HttpRequest() : method("POST"), timeout_ms(180000) {}

  /// @brief Full target URL.
  std::string url;

  /// @brief HTTP method (typically `"POST"`).
  std::string method;

  /// @brief HTTP headers as key-value pairs.
  std::map<std::string, std::string> headers;

  /// @brief Request body payload.
  std::string body;

  /// @brief Timeout in milliseconds.
  int64_t timeout_ms;
};

/// @brief Callback invoked when a unary HTTP request completes or when a
/// streaming HTTP request finishes.
///
/// @param status_code HTTP status code (e.g. 200), or 0 if a transport/network
/// error occurred before an HTTP response was received.
/// @param response_body Full response body for unary calls, or error body if
/// `status_code` is non-2xx during a streaming call.
/// @param transport_error Non-empty error message if a transport-level failure
/// or timeout occurred.
typedef std::function<void(int status_code, const std::string& response_body,
                           const std::string& transport_error)>
    HttpCompletionCallback;

/// @brief Callback invoked incrementally as raw response body bytes arrive from
/// the server during a streaming HTTP request (when HTTP status is 2xx).
///
/// @param data Pointer to the received bytes.
/// @param length Number of bytes in `data`.
/// @return `true` to continue receiving stream data, or `false` to abort.
typedef std::function<bool(const char* data, size_t length)>
    HttpStreamChunkCallback;

/// @brief Slim platform-specific HTTP transport interface.
///
/// Implemented per platform:
/// - Desktop / Linux / macOS / Windows: `libcurl` (`http_sender_desktop.cc`)
/// - Android: Slim JNI `java.net.HttpURLConnection` (`http_sender_android.cc`)
/// - iOS: Slim Objective-C++ `NSURLSession` (`http_sender_ios.mm`)
class HttpSender {
 public:
  /// @brief Initializes platform HTTP resources if needed (reference-counted).
  static void Initialize();

  /// @brief Cleans up platform HTTP resources if needed (reference-counted).
  static void Cleanup();

  /// @brief Sends an asynchronous unary HTTP request.
  ///
  /// @param app Owning `firebase::App` (used on Android to obtain `JNIEnv*`).
  /// @param request The HTTP request parameters.
  /// @param on_complete Callback invoked when the request completes.
  static void SendUnary(::firebase::App* app, const HttpRequest& request,
                        const HttpCompletionCallback& on_complete);

  /// @brief Sends an asynchronous streaming HTTP request, invoking `on_chunk`
  /// as response bytes arrive when the server responds with HTTP 2xx, and
  /// `on_complete` when the transfer finishes.
  ///
  /// @param app Owning `firebase::App` (used on Android to obtain `JNIEnv*`).
  /// @param request The HTTP request parameters.
  /// @param on_chunk Callback invoked as raw bytes arrive for HTTP 2xx streams.
  /// @param on_complete Callback invoked when the stream finishes or fails.
  static void SendStream(::firebase::App* app, const HttpRequest& request,
                         const HttpStreamChunkCallback& on_chunk,
                         const HttpCompletionCallback& on_complete);
};

}  // namespace internal
}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_COMMON_HTTP_SENDER_H_
