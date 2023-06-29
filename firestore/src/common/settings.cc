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

#include <ostream>
#include <sstream>

#include "app/meta/move.h"

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

void Settings::set_persistence_enabled(bool enabled) {
  persistence_enabled_ = enabled;
}

void Settings::set_cache_size_bytes(int64_t value) {
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

// Apple uses a different mechanism, defined in `settings_apple.mm`.
#if !defined(__APPLE__) && !defined(__ANDROID__)
std::unique_ptr<util::Executor> Settings::CreateExecutor() const {
  return util::Executor::CreateSerial("integration_tests");
}
#endif

}  // namespace firestore
}  // namespace firebase
