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
  settings_internal_ = std::make_unique<PersistentCacheSettingsInternal>(
      CorePersistentSettings{});
}

PersistentCacheSettings::~PersistentCacheSettings() {
  settings_internal_.reset();
}

PersistentCacheSettings::PersistentCacheSettings(
    const PersistentCacheSettingsInternal& other) {
  settings_internal_ = std::make_unique<PersistentCacheSettingsInternal>(other);
}

PersistentCacheSettings PersistentCacheSettings::WithSizeBytes(
    int64_t size) const {
  return {PersistentCacheSettingsInternal{
      settings_internal_->core_settings().WithSizeBytes(size)}};
}

MemoryCacheSettings MemoryCacheSettings::Create() { return {}; }

MemoryCacheSettings::MemoryCacheSettings() {
  settings_internal_ =
      std::make_unique<MemoryCacheSettingsInternal>(CoreMemorySettings{});
}

MemoryCacheSettings::~MemoryCacheSettings() { settings_internal_.reset(); }

MemoryCacheSettings::MemoryCacheSettings(
    const MemoryCacheSettingsInternal& other) {
  settings_internal_ = std::make_unique<MemoryCacheSettingsInternal>(other);
}
}  // namespace firestore
}  // namespace firebase
