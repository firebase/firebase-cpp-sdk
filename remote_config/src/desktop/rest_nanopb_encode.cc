// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "remote_config/src/desktop/rest_nanopb_encode.h"
#include "remote_config/src/desktop/rest_nanopb.h"

#include "app/meta/move.h"
#include "app/src/log.h"
#include "remote_config/config.pb.h"

#include "nanopb/pb.h"
#include "nanopb/pb_encode.h"

namespace firebase {
namespace remote_config {
namespace internal {

NPB_ALIAS_DEF(NpbFetchRequest, desktop_config_ConfigFetchRequest);
NPB_ALIAS_DEF(NpbPackageData, desktop_config_PackageData);
NPB_ALIAS_DEF(NpbNamedValue, desktop_config_NamedValue);

pb_ostream_t CreateOutputStream(std::string* destination) {
  pb_ostream_t stream;
  stream.callback = [](pb_ostream_t* stream, const uint8_t* buf, size_t count) {
    std::string* result = static_cast<std::string*>(stream->state);
    result->insert(result->end(), buf, buf + count);
    return true;
  };
  stream.state = destination;
  stream.max_size = SIZE_MAX;
  stream.bytes_written = 0;
  // zero out errmsg so that nanopb populates it upon errors.
  stream.errmsg = nullptr;
  return stream;
}

pb_callback_t EncodeStringCB(const std::string& source) {
  pb_callback_t callback;
  if (source.empty()) {
    callback.funcs.encode = nullptr;
    return callback;
  }

  callback.arg = const_cast<std::string*>(&source);
  callback.funcs.encode = [](pb_ostream_t* stream, const pb_field_t* field,
                             void* const* arg) {
    auto& str = *static_cast<const std::string*>(*arg);

    if (!pb_encode_tag_for_field(stream, field)) {
      return false;
    }
    return pb_encode_string(
        stream, reinterpret_cast<const pb_byte_t*>(str.c_str()), str.size());
  };
  return callback;
}

pb_callback_t EncodeNamedValuesCB(const NamedValues& source) {
  pb_callback_t callback;
  if (source.empty()) {
    callback.funcs.encode = nullptr;
    return callback;
  }

  callback.arg = const_cast<NamedValues*>(&source);
  callback.funcs.encode = [](pb_ostream_t* stream, const pb_field_t* field,
                             void* const* arg) {
    auto& source = *static_cast<const NamedValues*>(*arg);

    for (const auto& named_value : source) {
      NpbNamedValue npb_named_value = kDefaultNpbNamedValue;
      npb_named_value.name = EncodeStringCB(named_value.first);
      npb_named_value.value = EncodeStringCB(named_value.second);

      if (!pb_encode_tag_for_field(stream, field)) {
        return false;
      }
      if (!pb_encode_submessage(stream, kNpbNamedValueFields,
                                &npb_named_value)) {
        return false;
      }
    }
    return true;
  };
  return callback;
}

pb_callback_t EncodePackageDataCB(const PackageData& source) {
  pb_callback_t callback;
  callback.arg = const_cast<PackageData*>(&source);
  callback.funcs.encode = [](pb_ostream_t* stream, const pb_field_t* field,
                             void* const* arg) {
    // Note: we can't use lambda capture here, which would be nice, because the
    // nanopb function pointer is a c-style function pointer, not std::function.
    auto& source = *static_cast<const PackageData*>(*arg);

    NpbPackageData npb_package = kDefaultNpbPackageData;
    // Ordering by type rather than struct placement.

    npb_package.package_name = EncodeStringCB(source.package_name);
    npb_package.gmp_project_id = EncodeStringCB(source.gmp_project_id);

    npb_package.namespace_digest = EncodeNamedValuesCB(source.namespace_digest);
    npb_package.custom_variable = EncodeNamedValuesCB(source.custom_variable);
    npb_package.app_instance_id = EncodeStringCB(source.app_instance_id);
    npb_package.app_instance_id_token =
        EncodeStringCB(source.app_instance_id_token);
    npb_package.sdk_version = source.sdk_version;
    npb_package.has_sdk_version = (source.sdk_version != 0);
    npb_package.requested_cache_expiration_seconds =
        source.requested_cache_expiration_seconds;
    npb_package.has_requested_cache_expiration_seconds =
        (source.requested_cache_expiration_seconds != 0);
    npb_package.fetched_config_age_seconds = source.fetched_config_age_seconds;
    npb_package.has_fetched_config_age_seconds =
        (source.fetched_config_age_seconds != -1);
    npb_package.active_config_age_seconds = source.active_config_age_seconds;
    npb_package.has_active_config_age_seconds =
        (source.active_config_age_seconds != -1);

    if (!pb_encode_tag_for_field(stream, field)) {
      return false;
    }
    return pb_encode_submessage(stream, kNpbPackageDataFields, &npb_package);
  };
  return callback;
}

std::string EncodeFetchRequest(const ConfigFetchRequest& config_fetch_request) {
  std::string output;
  pb_ostream_t stream = CreateOutputStream(&output);

  NpbFetchRequest npb_request = kDefaultNpbFetchRequest;
  npb_request.client_version = config_fetch_request.client_version;
  npb_request.has_client_version = config_fetch_request.client_version != 0;
  npb_request.device_type = config_fetch_request.device_type;
  npb_request.has_device_type = config_fetch_request.device_type != 0;
  npb_request.device_subtype = config_fetch_request.device_subtype;
  npb_request.has_device_subtype = config_fetch_request.device_subtype != 0;

  npb_request.package_data =
      EncodePackageDataCB(config_fetch_request.package_data);

  if (!pb_encode(&stream, kNpbFetchRequestFields, &npb_request)) {
    LogError(stream.errmsg);
  }
  return Move(output);
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
