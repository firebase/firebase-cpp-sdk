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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_INTERFACE_H_
#define FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_INTERFACE_H_

#include <limits>
#include <memory>

#include "app/rest/controller_interface.h"
#include "app/rest/request.h"
#include "app/rest/response.h"
#include "flatbuffers/stl_emulation.h"

namespace firebase {
namespace rest {

class Transport {
 public:
  Transport() {}
  virtual ~Transport();

  // Perform a HTTP request and put result in response and return a Controller
  // in controller_out.
  // Deprecated.
  void Perform(const Request& request, Response* response,
               flatbuffers::unique_ptr<Controller>* controller_out) {
    PerformInternal(const_cast<Request*>(&request), response, controller_out);
  }

  // Perform a HTTP request and put result in response.
  // Deprecated.
  void Perform(const Request& request, Response* response) {
    PerformInternal(const_cast<Request*>(&request), response, nullptr);
  }

  // Perform a HTTP request and put result in response and return a Controller
  // in controller_out.
  void Perform(Request* request, Response* response,
               flatbuffers::unique_ptr<Controller>* controller_out) {
    PerformInternal(request, response, controller_out);
  }

 private:
  virtual void PerformInternal(
      Request* request, Response* response,
      flatbuffers::unique_ptr<Controller>* controller_out) = 0;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_INTERFACE_H_
