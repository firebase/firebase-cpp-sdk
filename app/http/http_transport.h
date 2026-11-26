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

// Cross-platform streaming HTTP transport backed by platform-native stacks:
//   - Desktop (Linux / macOS / Windows): libcurl multi-handle (sharing
//     `firebase_rest_lib`'s TLS policy and ref-counted curl initialization).
//   - Apple mobile (iOS / tvOS): ARC NSURLSession with bound-stream uploads.
//   - Android: JNI HttpURLConnection on a bounded worker thread pool.
//
// Supports full-duplex streaming uploads and downloads with bounded socket
// backpressure, immediate incremental delivery for Server-Sent Events (SSE),
// deterministic cancellation, and uniform RFC 3986 redirect/credential policy.

#ifndef FIREBASE_APP_HTTP_HTTP_TRANSPORT_H_
#define FIREBASE_APP_HTTP_HTTP_TRANSPORT_H_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__ANDROID__)
#include <jni.h>
#endif  // defined(__ANDROID__)

#include "app/http/http_bridge.h"

namespace firebase {
namespace http {

// Event kinds delivered through `HttpWaitNextEventInternal` in the low 8 bits
// of the packed `(handle << 8) | kind` return value.
enum HttpEventKind : int32_t {
  kHttpEventNone = 0,
  kHttpEventRequestSpaceAvailable = 1,
  kHttpEventResponseHeadersReady = 2,
  kHttpEventResponseBodyAvailable = 3,
};

// Bounded buffer limits per in-flight operation to preserve socket-level
// backpressure in both upload and download directions.
inline constexpr size_t kMaxRequestBufferBytes = 256 * 1024;
inline constexpr size_t kLowWaterRequestBytes = 128 * 1024;
inline constexpr size_t kMaxResponseBufferBytes = 2 * 1024 * 1024;
inline constexpr size_t kLowWaterResponseBytes = 1 * 1024 * 1024;

// Maximum number of HTTP redirects followed per operation across all backends.
inline constexpr int kMaxRedirects = 10;

// Special content_length sentinel indicating no request body is attached.
inline constexpr int64_t kNoRequestContent = -2;
// Special content_length sentinel indicating unknown-length chunked content.
inline constexpr int64_t kChunkedRequestContent = -1;

// Return codes from `HttpTryReadResponseBodyInternal`.
inline constexpr int32_t kHttpReadWouldBlock = -2;
inline constexpr int32_t kHttpReadErrorOrCanceled = -1;

struct ByteChunk {
  std::vector<uint8_t> data;
  size_t offset = 0;

  size_t remaining() const { return data.size() - offset; }
};

// Request parameters used when creating an `HttpOperation` from C++ directly.
struct HttpRequestOptions {
  std::string method = "GET";
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string proxy_url;
  int64_t content_length = kNoRequestContent;
};

// Shared state for a single HTTP request/response lifecycle, held via
// `std::shared_ptr` by both the handle table and the active platform backend so
// cancellation or handle destruction never races with native worker threads.
struct HttpOperation {
  int64_t id = 0;
  std::string method;
  std::string url;
  std::vector<std::pair<std::string, std::string>> request_headers;
  std::string proxy_url;
  int64_t content_length = kNoRequestContent;

  mutable std::mutex mutex;
  std::condition_variable req_cv;
  std::condition_variable resp_cv;

  // Request body streaming pipe (caller -> native backend).
  std::deque<ByteChunk> req_chunks;
  size_t req_buffered_bytes = 0;
  bool req_eof = false;
  bool req_space_armed = false;

  // Response header state (native backend -> caller).
  bool headers_ready = false;
  int32_t status_code = 0;
  std::string reason_phrase;
  std::vector<std::pair<std::string, std::string>> response_headers;
  bool resp_headers_armed = false;

  // Response body streaming pipe (native backend -> caller).
  std::deque<ByteChunk> resp_chunks;
  size_t resp_buffered_bytes = 0;
  bool resp_eof = false;
  bool resp_read_armed = false;

  // Terminal failure / cancellation / single-start state.
  std::atomic<bool> started{false};
  std::atomic<bool> canceled{false};
  bool failed = false;
  std::string error_message;

  // Platform backend hooks invoked outside `mutex` when state changes.
  std::function<void()> on_request_data_changed;
  std::function<void()> on_response_space_available;
  std::function<void()> on_cancel;

  bool IsCanceled() const { return canceled.load(std::memory_order_acquire); }

  // Called by the platform backend once final response status & headers arrive.
  void CommitResponseHeaders(
      int32_t status, std::string_view reason,
      std::vector<std::pair<std::string, std::string>> headers);

  // Called by the platform backend as response body bytes arrive.
  // Returns true if buffered bytes are now >= `kMaxResponseBufferBytes`,
  // telling the backend to pause socket reads until
  // `on_response_space_available` fires.
  bool PushResponseBytes(const uint8_t* bytes, size_t length);

  // Called by the platform backend when the response body finishes cleanly.
  void MarkResponseComplete();

  // Called by the platform backend when a transport error aborts the transfer.
  void MarkFailed(std::string_view message);

  // Non-blocking drain of up to `max_bytes` from the request body queue.
  // Sets `out_eof` to true when the queue is empty and the caller finished
  // writing.
  size_t PopRequestBytes(uint8_t* dest, size_t max_bytes, bool* out_eof);
};

// C++17 registry entry points backing both direct C++ callers and the flat
// SWIG bridge in `http_bridge.h`.
int64_t CreateOperation(HttpRequestOptions options);
std::shared_ptr<HttpOperation> FindOperation(int64_t handle);
void StartOperation(int64_t handle);
void CancelOperation(int64_t handle);
void DestroyOperation(int64_t handle);

// Enqueues an event onto the global completion queue consumed by
// `HttpWaitNextEventInternal`.
void PostCompletionEvent(int64_t handle, HttpEventKind kind);

// Platform-specific entry point implemented in
// `http_transport_{curl,apple,android}`.
void StartPlatformHttpOperation(const std::shared_ptr<HttpOperation>& op);

#if defined(__ANDROID__)
// Optionally registers the process `JavaVM*` early (e.g., from `JNI_OnLoad`).
// If not called explicitly, the Android backend falls back to resolving the
// active `JavaVM*` from `firebase::util::GetJNIEnvFromApp()`.
void SetAndroidJavaVM(JavaVM* vm);
#endif  // defined(__ANDROID__)

// Case-insensitive ASCII equality helper shared by platform backends.
bool AsciiEqualsIgnoreCase(std::string_view a, std::string_view b);

// Trims leading and trailing ASCII whitespace.
std::string_view TrimAsciiWhitespace(std::string_view s);

// Parses `Name\tValue\n` (or `Name: Value\n`) header lines into ordered pairs,
// preserving duplicate header names.
std::vector<std::pair<std::string, std::string>> ParsePackedHeaders(
    std::string_view packed);

struct UrlOrigin {
  std::string scheme;
  std::string host;
  int port = 0;
};

// Parses scheme (`http` or `https` only), lowercased host, and effective port
// (`80` / `443` when omitted) from an absolute HTTP(S) URL.
std::optional<UrlOrigin> ParseUrlOrigin(std::string_view url);

bool IsHttpOrHttpsUrl(std::string_view url);
bool IsHttpsUrl(std::string_view url);
bool IsSameOrigin(std::string_view url_a, std::string_view url_b);

// Returns true for headers that must never be forwarded across a cross-origin
// redirect (`Authorization`, `Proxy-Authorization`, `X-Firebase-AppCheck`,
// `x-goog-api-key`).
bool IsSensitiveRedirectHeader(std::string_view header_name);

// Resolves an HTTP `Location` header against `current_url`, rejecting
// non-HTTP(S) schemes and `https` -> `http` scheme downgrades.
std::optional<std::string> ResolveRedirectUrl(std::string_view current_url,
                                              std::string_view location);

// Parses `[http[s]://]host[:port][/...]` into `(host, port)` for proxy setup.
std::optional<std::pair<std::string, int>> ParseProxyHostPort(
    std::string_view proxy_url);

}  // namespace http
}  // namespace firebase

#endif  // FIREBASE_APP_HTTP_HTTP_TRANSPORT_H_
