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

PersistentCacheSettings PersistentCacheSettings::CreateFromCoreSettings(
    const CorePersistentSettings& core_settings) {
  auto result = PersistentCacheSettings{};
  result.settings_internal_ =
      std::make_unique<PersistentCacheSettingsInternal>(core_settings);
  return result;
}

PersistentCacheSettings::PersistentCacheSettings() {
  settings_internal_ = std::make_unique<PersistentCacheSettingsInternal>(
      CorePersistentSettings{});
}

PersistentCacheSettings::~PersistentCacheSettings() {
  settings_internal_.reset();
}

PersistentCacheSettings::PersistentCacheSettings(
    const PersistentCacheSettings& other) {
  settings_internal_ = std::make_unique<PersistentCacheSettingsInternal>(
      *other.settings_internal_);
}

PersistentCacheSettings& PersistentCacheSettings::operator=(
    const PersistentCacheSettings& other) {
  if (this == &other) {
    return *this;
  }
  settings_internal_ = std::make_unique<PersistentCacheSettingsInternal>(
      *other.settings_internal_);
  return *this;
}

PersistentCacheSettings PersistentCacheSettings::WithSizeBytes(
    int64_t size) const {
  PersistentCacheSettings new_settings{*this};
  new_settings.settings_internal_->set_core_settings(
      new_settings.settings_internal_->core_settings().WithSizeBytes(size));
  return new_settings;
}

const CoreCacheSettings& PersistentCacheSettings::core_cache_settings() const {
  return settings_internal_->core_settings();
}

MemoryEagerGCSettings MemoryEagerGCSettings::Create() { return {}; }

MemoryEagerGCSettings::MemoryEagerGCSettings() {
  settings_internal_ = std::make_unique<MemoryEagerGCSettingsInternal>(
      CoreMemoryEagerGcSettings{});
}

MemoryEagerGCSettings::~MemoryEagerGCSettings() { settings_internal_.reset(); }

MemoryLruGCSettings MemoryLruGCSettings::Create() { return {}; }

MemoryLruGCSettings::MemoryLruGCSettings() {
  settings_internal_ =
      std::make_unique<MemoryLruGCSettingsInternal>(CoreMemoryLruGcSettings{});
}

MemoryLruGCSettings::MemoryLruGCSettings(
    const MemoryLruGCSettingsInternal& other) {
  settings_internal_ = std::make_unique<MemoryLruGCSettingsInternal>(other);
}

MemoryLruGCSettings::~MemoryLruGCSettings() { settings_internal_.reset(); }

MemoryLruGCSettings MemoryLruGCSettings::WithSizeBytes(int64_t size) {
  return {MemoryLruGCSettingsInternal{
      settings_internal_->core_settings().WithSizeBytes(size)}};
}

MemoryCacheSettings MemoryCacheSettings::Create() { return {}; }

MemoryCacheSettings MemoryCacheSettings::CreateFromCoreSettings(
    const CoreMemorySettings& core_settings) {
  auto result = MemoryCacheSettings{};
  result.settings_internal_ =
      std::make_unique<MemoryCacheSettingsInternal>(core_settings);
  return result;
}

MemoryCacheSettings::MemoryCacheSettings() {
  settings_internal_ =
      std::make_unique<MemoryCacheSettingsInternal>(CoreMemorySettings{});
}

MemoryCacheSettings::MemoryCacheSettings(const MemoryCacheSettings& other) {
  settings_internal_ =
      std::make_unique<MemoryCacheSettingsInternal>(*other.settings_internal_);
}

MemoryCacheSettings& MemoryCacheSettings::operator=(
    const MemoryCacheSettings& other) {
  if (this == &other) {
    return *this;
  }

  settings_internal_ =
      std::make_unique<MemoryCacheSettingsInternal>(*other.settings_internal_);
  return *this;
}

MemoryCacheSettings::~MemoryCacheSettings() { settings_internal_.reset(); }

MemoryCacheSettings MemoryCacheSettings::WithGarbageCollectorSettings(
    const MemoryGarbageCollectorSettings& settings) const {
  MemoryCacheSettings result{*this};
  CoreMemorySettings core_settings = result.settings_internal_->core_settings();
  result.settings_internal_->set_core_settings(
      core_settings.WithMemoryGarbageCollectorSettings(
          settings.core_gc_settings()));
  return result;
}

const CoreCacheSettings& MemoryCacheSettings::core_cache_settings() const {
  return settings_internal_->core_settings();
}

bool operator==(const MemoryCacheSettings& lhs,
                const MemoryCacheSettings& rhs) {
  return &lhs == &rhs || (*lhs.settings_internal_ == *rhs.settings_internal_);
}

bool operator!=(const MemoryCacheSettings& lhs,
                const MemoryCacheSettings& rhs) {
  return !(lhs == rhs);
}

bool operator==(const PersistentCacheSettings& lhs,
                const PersistentCacheSettings& rhs) {
  return &lhs == &rhs || (*lhs.settings_internal_ == *rhs.settings_internal_);
}

bool operator!=(const PersistentCacheSettings& lhs,
                const PersistentCacheSettings& rhs) {
  return !(lhs == rhs);
}

bool operator==(const MemoryEagerGCSettings& lhs,
                const MemoryEagerGCSettings& rhs) {
  return &lhs == &rhs || (*lhs.settings_internal_ == *rhs.settings_internal_);
}

bool operator!=(const MemoryEagerGCSettings& lhs,
                const MemoryEagerGCSettings& rhs) {
  return !(lhs == rhs);
}

bool operator==(const MemoryLruGCSettings& lhs,
                const MemoryLruGCSettings& rhs) {
  return &lhs == &rhs || (*lhs.settings_internal_ == *rhs.settings_internal_);
}

bool operator!=(const MemoryLruGCSettings& lhs,
                const MemoryLruGCSettings& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase
