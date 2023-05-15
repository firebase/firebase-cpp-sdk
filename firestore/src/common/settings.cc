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

#include "firestore/src/include/firebase/firestore/settings.h"

#include <memory>
#include <ostream>
#include <sstream>

#include "Firestore/core/src/api/settings.h"
#include "Firestore/core/src/util/hard_assert.h"
#include "app/meta/move.h"
#include "firebase/firestore/local_cache_settings.h"
#include "firestore/src/common/exception_common.h"

#if !defined(__ANDROID__)
#include "Firestore/core/src/util/executor.h"
#endif

namespace firebase {
namespace firestore {

namespace {

const char kDefaultHost[] = "firestore.googleapis.com";

std::string ToStr(bool v) { return v ? "true" : "false"; }

std::string ToStr(int64_t v) {
  // TODO(b/192885139): when possible, use `std::to_string` instead.
  std::ostringstream s;
  s << v;
  return s.str();
}

}  // namespace

#if !defined(__APPLE__)
Settings::Settings() : host_(kDefaultHost) {}
#endif

void Settings::set_host(std::string host) { host_ = firebase::Move(host); }

void Settings::set_ssl_enabled(bool enabled) { ssl_enabled_ = enabled; }

LocalCacheSettings Settings::local_cache_settings() const {
  if (cache_settings_source_ == CacheSettingsSource::kNew) {
    return local_cache_settings_;
  } else if (is_persistence_enabled()) {
    return LocalCacheSettings().WithCacheSettings(LocalCacheSettings::PersistentCacheSettings().WithSizeBytes(cache_size_bytes()));
  } else {
    return LocalCacheSettings().WithCacheSettings(LocalCacheSettings::MemoryCacheSettings());
  }
}

void Settings::set_local_cache_settings(LocalCacheSettings cache) {
  if (cache_settings_source_ == CacheSettingsSource::kOld) {
    SimpleThrowIllegalState(
        "Cannot mix set_local_cache_settings() with legacy cache api like "
        "set_persistence_enabled() or set_cache_size_bytes()");
  }
  cache_settings_source_ = CacheSettingsSource::kNew;
  local_cache_settings_ = firebase::Move(cache);
}

void Settings::set_persistence_enabled(bool enabled) {
  if (cache_settings_source_ == CacheSettingsSource::kNew) {
    SimpleThrowIllegalState(
        "Cannot mix legacy cache api set_persistence_enabled() with new cache "
        "api set_local_cache_settings()");
  }

  cache_settings_source_ == CacheSettingsSource::kOld;
  persistence_enabled_ = enabled;
}

void Settings::set_cache_size_bytes(int64_t value) {
  if (cache_settings_source_ == CacheSettingsSource::kNew) {
    SimpleThrowIllegalState(
        "Cannot mix legacy cache api set_cache_size_bytes() with new cache api "
        "set_local_cache_settings()");
  }

  cache_settings_source_ = CacheSettingsSource::kOld;
  cache_size_bytes_ = value;
}

std::string Settings::ToString() const {
  return std::string("Settings(host='") + host() +
         "', is_ssl_enabled=" + ToStr(is_ssl_enabled()) +
         ", is_persistence_enabled=" + ToStr(is_persistence_enabled()) +
         ", cache_size_bytes=" + ToStr(cache_size_bytes()) + ")";
}

std::ostream& operator<<(std::ostream& out, const Settings& settings) {
  return out << settings.ToString();
}

bool operator==(const Settings& lhs, const Settings& rhs) {
  if (lhs.host() != rhs.host()) {
    return false;
  }
  if (lhs.is_ssl_enabled() != rhs.is_ssl_enabled()) {
    return false;
  }
  if (lhs.local_cache_settings() != rhs.local_cache_settings()) {
    return false;
  }
  #if defined(__OBJC__)
  if (lhs.dispatch_queue() != rhs.dispatch_queue()) {
    return false;
  }
  #endif  // defined(__OBJC__)
  return true;
}

// Apple uses a different mechanism, defined in `settings_apple.mm`.
#if !defined(__APPLE__) && !defined(__ANDROID__)
std::unique_ptr<util::Executor> Settings::CreateExecutor() const {
  return util::Executor::CreateSerial("integration_tests");
}
#endif

}  // namespace firestore
}  // namespace firebase
