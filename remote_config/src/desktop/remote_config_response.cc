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

#include "remote_config/src/desktop/remote_config_response.h"

#include "app/src/variant_util.h"

namespace firebase {
namespace remote_config {
namespace internal {

RemoteConfigResponse::RemoteConfigResponse(const char* schema)
    : ResponseJson(schema) {}

// Mark the response completed for both header and body.
void RemoteConfigResponse::MarkCompleted() {
  entries_ = Variant::Null();
  ResponseJson::MarkCompleted();
  const char* body = GetBody();
  if (!body || body[0] == '\0' || sdk_error_code() != 0) {
    // If the body of response flatbuffer is empty or invalid, early out.
    return;
  }
  const flatbuffers::FlatBufferBuilder& builder = parser_->builder_;
  if (builder.GetSize() == 0 || builder.GetBufferPointer() == nullptr) {
    return;
  }
  const fbs::Response* body_fbs =
      flatbuffers::GetRoot<fbs::Response>(builder.GetBufferPointer());

  if (body_fbs && body_fbs->entries() && body_fbs->entries()->size() > 0) {
    entries_ = util::FlexbufferToVariant(body_fbs->entries_flexbuffer_root());
  }
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
