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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_RESPONSE_H_
#define FIREBASE_APP_CLIENT_CPP_REST_RESPONSE_H_

#include <cstddef>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "app/rest/transfer_interface.h"
#include "app/rest/util.h"

namespace firebase {
namespace rest {

// The base class to deal with HTTP/REST response.
class Response : public Transfer {
 public:
  Response();
  virtual ~Response() {}

  // Note: remove if support for Visual Studio <2015 is no longer needed.
  // Prior to version 2015, Visual Studio didn't support implicitly
  // defined move constructors, so one has to be provided.
  // Linter is suppressed because it complains about unnecessary moves. In this
  // case, applying move is more future-proof, in case one of the variables
  // which is now a fundamental gets changed to a user-defined type further on.
  Response(Response&& rhs)
      : status_(std::move(rhs.status_)),                      // NOLINT
        header_completed_(std::move(rhs.header_completed_)),  // NOLINT
        body_completed_(std::move(rhs.body_completed_)),      // NOLINT
        sdk_error_code_(std::move(rhs.sdk_error_code_)),      // NOLINT
        fetch_time_(std::move(rhs.fetch_time_)),              // NOLINT
        header_(std::move(rhs.header_)),
        body_(std::move(rhs.body_)),
        body_cache_(std::move(rhs.body_cache_)) {}

  // Process headers. Return false when it fails and will interrupt the request.
  virtual bool ProcessHeader(const char* buffer, size_t length);

  // Process body. Returns false when it fails and will interrupt the request.
  virtual bool ProcessBody(const char* buffer, size_t length);

  // Mark the response completed for both header and body.
  void MarkCompleted() override {
    // Make sure the fetch_time_ is always reasonable even when the response
    // does not have a valid Date header.
    if (fetch_time_ <= 0) {
      fetch_time_ = std::time(nullptr);
    }
    header_completed_ = true;
    body_completed_ = true;
  }

  // Marks the response as failed. There will never be a response, so stop
  // waiting for one.
  void MarkFailed() override {
    header_completed_ = false;
    body_completed_ = false;
  }

  // Getters.
  int status() const { return status_; }
  bool header_completed() const { return header_completed_; }
  bool body_completed() const { return body_completed_; }
  int sdk_error_code() const { return sdk_error_code_; }
  std::time_t fetch_time() const { return fetch_time_; }

  // Setters.
  void set_status(int status) { status_ = status; }

  void set_sdk_error_code(int sdk_error_code) {
    sdk_error_code_ = sdk_error_code;
  }

  // Get the field value for the specific field name in header. If no such field
  // is found in the header, return nullptr.
  const char* GetHeader(const char* name);

  // Get the body. If no body line has been received yet, return empty string.
  const char* GetBody() const;

  // Get the body. Use for binary body.
  virtual void GetBody(const char** data, size_t* size) const;

 private:
  // The status code of the response.
  int status_;
  // Whether all headers have been received.
  bool header_completed_;
  // Whether the entire body has been received.
  bool body_completed_;
  // When there is an SDK error, this is the error code. Otherwise, it is 0.
  int sdk_error_code_;
  // When we start to receive response.
  std::time_t fetch_time_;
  // Stores key-value pairs in header.
  std::map<std::string, std::string> header_;
  // Stores body in pieces and as a whole.
  std::vector<std::string> body_;
  mutable std::string body_cache_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_RESPONSE_H_
