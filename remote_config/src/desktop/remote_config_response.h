/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_RESPONSE_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_RESPONSE_H_

#include "app/rest/response_json.h"
#include "app/src/include/firebase/variant.h"
#include "remote_config/response_generated.h"
#include "remote_config/response_resource.h"

namespace firebase {
namespace remote_config {
namespace internal {

// Copied from App's variant_util, because of problems with Blastdoor.
// Convert from a Flexbuffer reference to a Variant.
Variant FlexbufferToVariant(const flexbuffers::Reference& ref);
// Convert from a Flexbuffer map to a Variant.
Variant FlexbufferMapToVariant(const flexbuffers::Map& map);
// Convert from a Flexbuffer vector to a Variant.
Variant FlexbufferVectorToVariant(const flexbuffers::Vector& vector);

class RemoteConfigResponse
    : public firebase::rest::ResponseJson<fbs::Response, fbs::ResponseT> {
 public:
  RemoteConfigResponse() : ResponseJson(response_resource_data) {}
  explicit RemoteConfigResponse(const char* schema);

  explicit RemoteConfigResponse(const unsigned char* schema)
      : RemoteConfigResponse(reinterpret_cast<const char*>(schema)) {}

  void MarkCompleted() override;

  Variant GetEntries();

  bool StatusMatch(std::string status_name) {
    return application_data_->state == status_name;
  }

 private:
  Variant entries_;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_RESPONSE_H_
