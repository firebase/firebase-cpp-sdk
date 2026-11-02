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

#ifndef FIREBASE_APP_REST_CURL_TRANSFER_H_
#define FIREBASE_APP_REST_CURL_TRANSFER_H_

#include "curl/curl.h"

namespace firebase {
namespace rest {
namespace curl {

// One libcurl easy handle driven by the shared CurlIoLoop.
//
// A Transfer is the unit of work the I/O loop understands: it owns (or at
// least has exclusive use of) one fully configured CURL easy handle and is
// told when libcurl considers that handle finished. Everything else - what
// the handle does, where the bytes go, how the owner is informed - is up to
// the implementation (see RestTransfer in app/rest/transport_curl.cc).
//
// Lifetime and threading contract:
//  * All methods are invoked on the I/O thread only.
//  * The Transfer must stay alive from CurlIoLoop::Add() until either
//    CurlIoLoop::Remove() returns or OnDone() has been called, whichever
//    happens first. After OnDone() the loop holds no reference to it, so
//    OnDone() may `delete this`.
//  * easy() must return the same handle for the whole time the transfer is
//    registered with the loop.
class Transfer {
 public:
  virtual ~Transfer();

  // The easy handle this transfer drives. Must already be configured when the
  // transfer is passed to CurlIoLoop::Add().
  virtual CURL* easy() = 0;

  // Called exactly once on the I/O thread when libcurl reports the transfer
  // finished (CURLMSG_DONE) or when the loop is shutting down with the
  // transfer still in flight (result == CURLE_ABORTED_BY_CALLBACK). The easy
  // handle has already been removed from the multi handle and the loop has
  // forgotten the Transfer, so it is safe to reuse or destroy the handle and
  // to destroy the Transfer itself from here.
  //
  // IMPORTANT: this runs on the single process-wide I/O thread that services
  // every libcurl transfer in the process. Implementations MUST keep it short:
  // record the result, hand off to the owner's own executor / scheduler /
  // semaphore, and return. Never block, never perform network I/O, and never
  // run product or application code here - doing so stalls every other
  // transfer in the process (head-of-line blocking).
  //
  // (For backward compatibility RestTransfer still invokes
  // Response::MarkCompleted()/MarkFailed() directly from OnDone(), exactly as
  // the previous CurlThread did. Response subclasses that do heavy work there
  // are documented in the Phase 1 audit and should move that work onto their
  // own executor.)
  virtual void OnDone(CURLcode result) = 0;
};

}  // namespace curl
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CURL_TRANSFER_H_
