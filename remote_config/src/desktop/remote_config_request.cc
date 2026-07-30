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

#include "remote_config/src/desktop/remote_config_request.h"

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/variant_util.h"

namespace firebase {
namespace remote_config {
namespace internal {

RemoteConfigRequest::RemoteConfigRequest(const char* schema)
    : RequestJson(schema), custom_signals_() {
  add_header(app_common::kApiClientHeader, App::GetUserAgent());
}

void RemoteConfigRequest::UpdatePostFields() {
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(fbs::Request::Pack(builder, application_data_.get()));

  std::string json;
  bool generate_status =
      GenerateText(*parser_, builder.GetBufferPointer(), &json) == nullptr;
  FIREBASE_ASSERT_RETURN_VOID(generate_status);

  // If custom signals are set, inject them into the POST request body JSON payload.
  if (!custom_signals_.empty()) {
    std::map<Variant, Variant> variant_map;
    for (const auto& kv : custom_signals_) {
      variant_map[Variant(kv.first)] = Variant(kv.second);
    }
    std::string custom_signals_json = util::VariantToJson(variant_map);

    // Insert the "custom_signals" field before the closing brace of the JSON object.
    size_t last_brace = json.rfind('}');
    if (last_brace != std::string::npos) {
      size_t first_brace = json.find('{');
      std::string insertion;
      if (first_brace != std::string::npos &&
          json.find_first_not_of(" \t\n\r", first_brace + 1) != last_brace) {
        insertion = ",\n  \"custom_signals\": " + custom_signals_json + "\n";
      } else {
        insertion = "\n  \"custom_signals\": " + custom_signals_json + "\n";
      }
      json.insert(last_brace, insertion);
    }
  }

  set_post_fields(json.c_str());
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
