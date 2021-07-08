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

#ifndef FIREBASE_APP_REST_TRANSPORT_MOCK_H_
#define FIREBASE_APP_REST_TRANSPORT_MOCK_H_

#include "app/rest/transport_interface.h"

namespace firebase {
namespace rest {
// Implement the mock transport layer without network connection.
class TransportMock : public Transport {
 public:
  TransportMock() {}
  ~TransportMock() override {}

  // Mock a HTTP request and put specified result in response.
  void PerformInternal(
      Request* request, Response* response,
      flatbuffers::unique_ptr<Controller>* controller_out) override;
};

}  // namespace rest
}  // namespace firebase
#endif  // FIREBASE_APP_REST_TRANSPORT_MOCK_H_
