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

// Flat handle-based C++/SWIG bridge for the cross-platform streaming HTTP
// transport (`firebase_http_transport`).
//
// Deliberately keeps its header surface restricted to primitive types and
// `std::string` so language bindings (such as Unity C# SWIG) can include it
// directly without exposing C++17 STL containers or native-to-managed reverse
// function-pointer callbacks.

#ifndef FIREBASE_APP_HTTP_HTTP_BRIDGE_H_
#define FIREBASE_APP_HTTP_HTTP_BRIDGE_H_

#include <stdint.h>

#include <string>

namespace firebase {

// Creates a new HTTP operation handle without starting it.
// `headers_packed` encodes headers as `Name\tValue\n` lines.
// `content_length` uses `-2` for no request content, `-1` for unknown-length
// chunked streaming content, or `>= 0` for a known fixed byte length.
int64_t HttpCreateRequestInternal(const char* method, const char* url,
                                  const char* headers_packed,
                                  const char* proxy_url,
                                  int64_t content_length);

// Starts the platform-native HTTP operation for `handle`.
void HttpStartRequestInternal(int64_t handle);

// Non-blocking write of up to `length` request body bytes into the bounded
// upload pipe. Returns bytes accepted (`0` when full), or `-1` on failure,
// invalid arguments, early server response, or cancellation.
int32_t HttpTryWriteRequestBodyInternal(int64_t handle, void* data,
                                        int32_t length);

// Marks the end of the request body stream for `handle`.
void HttpFinishRequestBodyInternal(int64_t handle);

// Polls response header readiness. Returns HTTP status code (`> 0`) once
// headers are committed, `0` if still pending (arming a headers-ready event),
// or `-1` on transport error or cancellation.
int32_t HttpPollResponseHeadersInternal(int64_t handle);

// Returns the committed response reason phrase and headers packed as:
//   `<reason_phrase>\n<Name>\t<Value>\n...`
std::string HttpGetResponseHeadersInternal(int64_t handle);

// Non-blocking read of up to `max_length` response body bytes. Returns bytes
// copied (`> 0`), `0` on clean EOF, `-2` if no bytes are buffered yet (arming
// a body-available event), or `-1` on invalid arguments, transport error, or
// cancellation.
int32_t HttpTryReadResponseBodyInternal(int64_t handle, void* dest,
                                        int32_t max_length);

// Returns the terminal transport error message for `handle`, if any.
std::string HttpGetErrorInternal(int64_t handle);

// Returns the structured error class for `handle`:
//   0 = None
//   1 = Canceled
//   2 = Connect (DNS, refused, reset, unreachable, I/O abort)
//   3 = TLS (certificate verification, handshake, SSL policy failure)
//   4 = Timeout (connect or transfer timeout)
//   5 = Other (invalid request/arguments, redirect policy, protocol, unknown)
int32_t HttpGetErrorClassInternal(int64_t handle);

// Cancels an in-flight HTTP operation immediately and wakes any armed waiters.
void HttpCancelRequestInternal(int64_t handle);

// Cancels (if active) and removes `handle` from the operation registry.
void HttpDestroyRequestInternal(int64_t handle);

// Cancels and removes all active HTTP operations from the registry and wakes
// the event pump (for example on Editor domain reload or shutdown).
void HttpCancelAllRequestsInternal();

// Waits up to `timeout_ms` (`< 0` for infinite, `0` for non-blocking poll) for
// the next completion event. Returns `(handle << 8) | event_kind`, or `0` on
// timeout / explicit wake.
int64_t HttpWaitNextEventInternal(int32_t timeout_ms);

// Wakes a thread blocked in `HttpWaitNextEventInternal` (e.g. on shutdown or
// domain unload).
void HttpWakeEventPumpInternal();

}  // namespace firebase

#endif  // FIREBASE_APP_HTTP_HTTP_BRIDGE_H_
