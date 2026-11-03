// Copyright 2017 Google LLC
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

#include "remote_config/src/desktop/metadata.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "flatbuffers/flexbuffers.h"
#include "remote_config/src/include/firebase/remote_config.h"

namespace firebase {
namespace remote_config {
namespace internal {

RemoteConfigMetadata::RemoteConfigMetadata()
    : info_({0, kLastFetchStatusPending, kFetchFailureReasonInvalid, 0}) {}

std::string RemoteConfigMetadata::Serialize() const {
  flexbuffers::Builder fbb;
  // Write out the struct members as a map.
  fbb.Map([&]() {
    // ConfigInfo struct
    fbb.Map("info", [&]() {
      fbb.UInt("fetch_time", info_.fetch_time);
      // LastFetchStatus enum
      fbb.Int("last_fetch_status", static_cast<int>(info_.last_fetch_status));
      // FetchFailureReason enum
      fbb.Int("last_fetch_failure_reason",
              static_cast<int>(info_.last_fetch_failure_reason));
      fbb.UInt("throttled_end_time", info_.throttled_end_time);
    });

    fbb.Map("digest_by_namespace", [&]() {
      for (const auto& kv : digest_by_namespace_) {
        fbb.String(kv.first.c_str(), kv.second);
      }
    });

    fbb.Map("settings", [&]() {
      for (const auto& setting : settings_) {
        fbb.String(std::to_string(setting.first).c_str(), setting.second);
      }
    });

    fbb.Map("custom_signals", [&]() {
      for (const auto& signal : custom_signals_) {
        const std::string& key = signal.first;
        const Variant& val = signal.second;
        if (val.is_mutable_string()) {
          fbb.String(key.c_str(), val.mutable_string());
        } else if (val.is_static_string()) {
          const char* s = val.string_value();
          fbb.String(key.c_str(), s ? s : "");
        } else if (val.is_int64()) {
          fbb.Int(key.c_str(), val.int64_value());
        } else if (val.is_double()) {
          fbb.Double(key.c_str(), val.double_value());
        }
      }
    });
  });
  fbb.Finish();
  const std::vector<uint8_t>& buffer = fbb.GetBuffer();
  return std::string(buffer.cbegin(), buffer.cend());
}

bool RemoteConfigMetadata::Deserialize(const std::string& buffer) {
  if (buffer.empty()) {
    return false;
  }
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
  size_t size = buffer.size();
  if (!flexbuffers::VerifyBuffer(data, size)) {
    return false;
  }
  flexbuffers::Reference root = flexbuffers::GetRoot(data, size);
  if (!root.IsMap()) {
    return false;
  }
  auto struct_map = root.AsMap();

  flexbuffers::Map info = struct_map["info"].AsMap();
  info_.fetch_time = info["fetch_time"].AsUInt64();
  info_.last_fetch_status =
      static_cast<LastFetchStatus>(info["last_fetch_status"].AsInt32());
  info_.last_fetch_failure_reason = static_cast<FetchFailureReason>(
      info["last_fetch_failure_reason"].AsInt32());
  info_.throttled_end_time = info["throttled_end_time"].AsUInt64();

  digest_by_namespace_.clear();
  flexbuffers::Map digests = struct_map["digest_by_namespace"].AsMap();
  DeserializeMap(&digest_by_namespace_, digests);

  settings_.clear();
  flexbuffers::Map settings = struct_map["settings"].AsMap();
  for (size_t i = 0, n = settings.size(); i < n; ++i) {
    const char* key_str = settings.Keys()[i].AsKey();
    if (!key_str) continue;
    char* endptr = nullptr;
    errno = 0;
    long raw_key = std::strtol(key_str, &endptr, 10);
    if (endptr == key_str || *endptr != '\0' || errno == ERANGE) {
      continue;
    }
    if (raw_key < std::numeric_limits<int>::min() ||
        raw_key > std::numeric_limits<int>::max()) {
      continue;
    }
    int int_key = static_cast<int>(raw_key);
    flexbuffers::String val = settings.Values()[i].AsString();
    settings_[static_cast<ConfigSetting>(int_key)] =
        std::string(val.c_str(), val.length());
  }

  custom_signals_.clear();
  flexbuffers::Map custom_signals = struct_map["custom_signals"].AsMap();
  for (size_t i = 0, n = custom_signals.size(); i < n; ++i) {
    const char* key_str = custom_signals.Keys()[i].AsKey();
    if (!key_str) continue;
    flexbuffers::Reference val_ref = custom_signals.Values()[i];
    if (val_ref.IsString()) {
      flexbuffers::String val = val_ref.AsString();
      custom_signals_[key_str] =
          Variant::FromMutableString(std::string(val.c_str(), val.length()));
    } else if (val_ref.IsInt()) {
      custom_signals_[key_str] = Variant(val_ref.AsInt64());
    } else if (val_ref.IsFloat()) {
      custom_signals_[key_str] = Variant(val_ref.AsDouble());
    }
  }
  return true;
}

void RemoteConfigMetadata::AddSetting(const ConfigSetting& setting,
                                      const std::string& value) {
  settings_[setting] = value;
}

// Now we use only kConfigSettingDeveloperMode for settings
// "0" means that developer mode is disable, "1" means developer mode is enable
std::string RemoteConfigMetadata::GetSetting(
    const ConfigSetting& setting) const {
  auto it = settings_.find(setting);
  return it == settings_.end() ? "0" : it->second;
}

bool RemoteConfigMetadata::operator==(const RemoteConfigMetadata& right) const {
  return digest_by_namespace_ == right.digest_by_namespace_ &&
         settings_ == right.settings_ &&
         custom_signals_ == right.custom_signals_ &&
         info_.fetch_time == right.info_.fetch_time &&
         info_.last_fetch_status == right.info_.last_fetch_status &&
         info_.last_fetch_failure_reason ==
             right.info_.last_fetch_failure_reason &&
         info_.throttled_end_time == right.info_.throttled_end_time;
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
