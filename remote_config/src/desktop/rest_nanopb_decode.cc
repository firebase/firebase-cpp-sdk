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

#include "remote_config/src/desktop/rest_nanopb_decode.h"
#include "remote_config/src/desktop/rest_nanopb.h"
#include "app/meta/move.h"
#include "nanopb/pb.h"
#include "nanopb/pb_decode.h"

namespace firebase {
namespace remote_config {
namespace internal {

NPB_ALIAS_DEF(NpbFetchResponse, desktop_config_ConfigFetchResponse);
NPB_ALIAS_DEF(NpbAppConfig, desktop_config_AppConfigTable);
NPB_ALIAS_DEF(NpbAppNamespaceConfig, desktop_config_AppNamespaceConfigTable);
NPB_ALIAS_DEF(NpbKeyValue, desktop_config_KeyValue);

pb_istream_t CreateInputStream(const std::string& source) {
  return pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t*>(source.c_str()), source.size());
}

pb_callback_t DecodeStringCB(std::string* destination) {
  pb_callback_t callback;
  callback.arg = destination;
  callback.funcs.decode = [](pb_istream_t* stream, const pb_field_t* field,
                             void** arg) {
    const char* value = reinterpret_cast<const char*>(stream->state);
    const size_t size = stream->bytes_left;

    auto* destination = static_cast<std::string*>(*arg);
    if (!pb_read(stream, nullptr, size)) {
      *destination = "";
      return false;
    }

    *destination = std::string(value, size);
    return true;
  };
  return callback;
}

pb_callback_t DecodeKeyValueCB(std::vector<KeyValue>* destination) {
  pb_callback_t callback;
  callback.arg = destination;
  callback.funcs.decode = [](pb_istream_t* stream, const pb_field_t* field,
                             void** arg) {
    // temporary storage
    KeyValue key_value;
    NpbKeyValue npb_key_value = kDefaultNpbKeyValue;

    npb_key_value.key = DecodeStringCB(&key_value.key);
    // The `value` in the proto is "bytes", which is compatible with string.
    npb_key_value.value = DecodeStringCB(&key_value.value);

    if (!pb_decode(stream, kNpbKeyValueFields, &npb_key_value)) {
        return false;
    }

    auto* destination = static_cast<std::vector<KeyValue>*>(*arg);
    destination->push_back(Move(key_value));
    return true;
  };
  return callback;
}

pb_callback_t DecodeAppNamespaceConfigCB(
    std::vector<AppNamespaceConfig>* destination) {
  pb_callback_t callback;
  callback.arg = destination;
  callback.funcs.decode = [](pb_istream_t* stream, const pb_field_t* field,
                             void** arg) {
    // temporary storage
    AppNamespaceConfig ns_config;

    NpbAppNamespaceConfig npb_ns_config = kDefaultNpbAppNamespaceConfig;
    npb_ns_config.namespace_but_not_a_cpp_reserved_word =
        DecodeStringCB(&ns_config.config_namespace);
    npb_ns_config.digest = DecodeStringCB(&ns_config.digest);
    npb_ns_config.entry = DecodeKeyValueCB(&ns_config.key_values);

    if (!pb_decode(stream, kNpbAppNamespaceConfigFields, &npb_ns_config)) {
      return false;
    }

    ns_config.status = npb_ns_config.status;
    auto* destination = static_cast<std::vector<AppNamespaceConfig>*>(*arg);
    destination->push_back(Move(ns_config));
    return true;
  };
  return callback;
}

pb_callback_t DecodeAppConfigCB(std::vector<AppConfig>* destination) {
  pb_callback_t callback;
  callback.arg = destination;
  callback.funcs.decode = [](pb_istream_t* stream, const pb_field_t* field,
                             void** arg) {
    AppConfig app_config;

    NpbAppConfig npb_app_config = kDefaultNpbAppConfig;
    npb_app_config.app_name = DecodeStringCB(&app_config.app_name);
    npb_app_config.namespace_config =
        DecodeAppNamespaceConfigCB(&app_config.ns_configs);

    if (!pb_decode(stream, kNpbAppConfigFields, &npb_app_config)) {
        return false;
    }

    auto* destination = static_cast<std::vector<AppConfig>*>(*arg);
    destination->push_back(Move(app_config));
    return true;
  };
  return callback;
}

bool DecodeResponse(ConfigFetchResponse* destination,
                    const std::string& proto_str) {
  pb_istream_t stream = CreateInputStream(proto_str);

  ConfigFetchResponse response;

  NpbFetchResponse npb_response = kDefaultNpbFetchResponse;
  npb_response.app_config = DecodeAppConfigCB(&response.configs);

  // Decode the stream triggering the callbacks to capture the data.
  if (!pb_decode(&stream, kNpbFetchResponseFields, &npb_response)) {
      return false;
  }

  *destination = Move(response);
  return true;
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
