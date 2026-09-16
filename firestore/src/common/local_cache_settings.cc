/*
 * Copyright 2023 Google LLC
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

#include "firestore/src/include/firebase/firestore/local_cache_settings.h"
#include <memory>

#include "firestore/src/common/hard_assert_common.h"
#if defined(__ANDROID__)
#else
#include "firestore/src/main/local_cache_settings_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

PersistentCacheSettings PersistentCacheSettings::Create() { return {}; }

PersistentCacheSettings::PersistentCacheSettings() {
  settings_internal_ = std::make_shared<PersistentCacheSettingsInternal>();
}

PersistentCacheSettings PersistentCacheSettings::WithSizeBytes(
    int64_t size) const {
  PersistentCacheSettings new_settings;
  new_settings.settings_internal_ =
      std::make_shared<PersistentCacheSettingsInternal>(
          this->settings_internal_->WithSizeBytes(size));
  return new_settings;
}

int64_t PersistentCacheSettings::size_bytes() const {
  return settings_internal_->size_bytes();
}

const LocalCacheSettingsInternal& PersistentCacheSettings::internal() const {
  return *settings_internal_;
}

MemoryEagerGCSettings MemoryEagerGCSettings::Create() { return {}; }

MemoryEagerGCSettings::MemoryEagerGCSettings() {
  settings_internal_ = std::make_shared<MemoryEagerGCSettingsInternal>();
}

const MemoryGarbageCollectorSettingsInternal& MemoryEagerGCSettings::internal()
    const {
  return *settings_internal_;
}

MemoryLruGCSettings MemoryLruGCSettings::Create() { return {}; }

MemoryLruGCSettings::MemoryLruGCSettings() {
  settings_internal_ = std::make_shared<MemoryLruGCSettingsInternal>();
}

MemoryLruGCSettings::MemoryLruGCSettings(
    const MemoryLruGCSettingsInternal& other) {
  settings_internal_ = std::make_shared<MemoryLruGCSettingsInternal>(other);
}

MemoryLruGCSettings MemoryLruGCSettings::WithSizeBytes(int64_t size) {
  MemoryLruGCSettings result;
  result.settings_internal_ = std::make_shared<MemoryLruGCSettingsInternal>(
      this->settings_internal_->WithSizeBytes(size));
  return result;
}

int64_t MemoryLruGCSettings::size_bytes() const {
  return settings_internal_->size_bytes();
}

const MemoryGarbageCollectorSettingsInternal& MemoryLruGCSettings::internal()
    const {
  return *settings_internal_;
}

MemoryCacheSettings MemoryCacheSettings::Create() { return {}; }

MemoryCacheSettings::MemoryCacheSettings() {
  settings_internal_ = std::make_shared<MemoryCacheSettingsInternal>();
}

MemoryCacheSettings MemoryCacheSettings::WithGarbageCollectorSettings(
    const MemoryGarbageCollectorSettings& settings) const {
  MemoryCacheSettings result;
  result.settings_internal_ = std::make_shared<MemoryCacheSettingsInternal>(
      this->settings_internal_->WithGarbageCollectorSettings(settings));
  return result;
}

const LocalCacheSettingsInternal& MemoryCacheSettings::internal() const {
  return *settings_internal_;
}

bool operator==(const LocalCacheSettings& lhs, const LocalCacheSettings& rhs) {
  return lhs.kind() == rhs.kind() && lhs.internal() == rhs.internal();
}

bool operator==(const MemoryCacheSettings& lhs,
                const MemoryCacheSettings& rhs) {
  return &lhs == &rhs || (*lhs.settings_internal_ == *rhs.settings_internal_);
}

bool operator==(const PersistentCacheSettings& lhs,
                const PersistentCacheSettings& rhs) {
  return &lhs == &rhs || (*lhs.settings_internal_ == *rhs.settings_internal_);
}

bool operator==(const MemoryGarbageCollectorSettings& lhs,
                const MemoryGarbageCollectorSettings& rhs) {
  return lhs.internal() == rhs.internal();
}

bool operator==(const MemoryEagerGCSettings& lhs,
                const MemoryEagerGCSettings& rhs) {
  return &lhs == &rhs || (*lhs.settings_internal_ == *rhs.settings_internal_);
}

bool operator==(const MemoryLruGCSettings& lhs,
                const MemoryLruGCSettings& rhs) {
  return &lhs == &rhs || (*lhs.settings_internal_ == *rhs.settings_internal_);
}

}  // namespace firestore
}  // namespace firebase
