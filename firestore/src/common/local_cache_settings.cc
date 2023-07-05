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

#include "Firestore/core/src/api/settings.h"
#include "firestore/src/common/hard_assert_common.h"
#if defined(__ANDROID__)
#else
#include "firestore/src/main/local_cache_settings_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

namespace {
using CoreCacheSettings = api::LocalCacheSettings;
using CorePersistentSettings = api::PersistentCacheSettings;
using CoreMemorySettings = api::MemoryCacheSettings;
using CoreMemoryGarbageCollectorSettings = api::MemoryGargabeCollectorSettings;
using CoreMemoryEagerGcSettings = api::MemoryEagerGcSettings;
using CoreMemoryLruGcSettings = api::MemoryLruGcSettings;
}  // namespace

PersistentCacheSettings PersistentCacheSettings::Create() { return {}; }

PersistentCacheSettings::PersistentCacheSettings() {
  settings_internal_ = std::make_shared<PersistentCacheSettingsInternal>(
      CorePersistentSettings{});
}

PersistentCacheSettings PersistentCacheSettings::WithSizeBytes(
    int64_t size) const {
  PersistentCacheSettings new_settings{*this};
  new_settings.settings_internal_->set_core_settings(
      new_settings.settings_internal_->core_settings().WithSizeBytes(size));
  return new_settings;
}

int64_t PersistentCacheSettings::size_bytes() const {
  return settings_internal_->core_settings().size_bytes();
}

const CoreCacheSettings& PersistentCacheSettings::core_settings() const {
  return settings_internal_->core_settings();
}

MemoryEagerGCSettings MemoryEagerGCSettings::Create() { return {}; }

MemoryEagerGCSettings::MemoryEagerGCSettings() {
  settings_internal_ = std::make_shared<MemoryEagerGCSettingsInternal>(
      CoreMemoryEagerGcSettings{});
}

const CoreMemoryGarbageCollectorSettings& MemoryEagerGCSettings::core_settings()
    const {
  return settings_internal_->core_settings();
}

MemoryLruGCSettings MemoryLruGCSettings::Create() { return {}; }

MemoryLruGCSettings::MemoryLruGCSettings() {
  settings_internal_ =
      std::make_shared<MemoryLruGCSettingsInternal>(CoreMemoryLruGcSettings{});
}

MemoryLruGCSettings::MemoryLruGCSettings(
    const MemoryLruGCSettingsInternal& other) {
  settings_internal_ = std::make_shared<MemoryLruGCSettingsInternal>(other);
}

MemoryLruGCSettings MemoryLruGCSettings::WithSizeBytes(int64_t size) {
  return {MemoryLruGCSettingsInternal{
      settings_internal_->core_settings().WithSizeBytes(size)}};
}

int64_t MemoryLruGCSettings::size_bytes() const {
  return settings_internal_->core_settings().size_bytes();
}

const CoreMemoryGarbageCollectorSettings& MemoryLruGCSettings::core_settings()
    const {
  return settings_internal_->core_settings();
}

MemoryCacheSettings MemoryCacheSettings::Create() { return {}; }

MemoryCacheSettings::MemoryCacheSettings() {
  settings_internal_ =
      std::make_shared<MemoryCacheSettingsInternal>(CoreMemorySettings{});
}

MemoryCacheSettings MemoryCacheSettings::WithGarbageCollectorSettings(
    const MemoryGarbageCollectorSettings& settings) const {
  MemoryCacheSettings result{*this};
  CoreMemorySettings core_settings = result.settings_internal_->core_settings();
  result.settings_internal_->set_core_settings(
      core_settings.WithMemoryGarbageCollectorSettings(
          settings.core_settings()));
  return result;
}

const CoreCacheSettings& MemoryCacheSettings::core_settings() const {
  return settings_internal_->core_settings();
}

bool operator==(const LocalCacheSettings& lhs, const LocalCacheSettings& rhs) {
  return lhs.kind() == rhs.kind() && lhs.core_settings() == rhs.core_settings();
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
  return lhs.core_settings() == rhs.core_settings();
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
