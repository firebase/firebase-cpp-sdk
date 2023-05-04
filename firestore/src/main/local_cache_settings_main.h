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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_LOCAL_CACHE_SETINGS_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_LOCAL_CACHE_SETINGS_MAIN_H_

#include <cstdint>

#include "Firestore/core/src/api/settings.h"

namespace firebase {
namespace firestore {

using CoreMemorySettings = api::MemoryCacheSettings;
using CorePersistentSettings = api::PersistentCacheSettings;

class LocalCacheSettingsInternal {};

class PersistentCacheSettingsInternal final
    : public LocalCacheSettingsInternal {
 public:
  explicit PersistentCacheSettingsInternal(
      const CorePersistentSettings& core_settings)
      : settings_(std::move(core_settings)) {}
  const CorePersistentSettings& core_settings() { return settings_; }
  void set_core_settings(const CorePersistentSettings& settings) {
    settings_ = settings;
  }

 private:
  CorePersistentSettings settings_;
};

class MemoryCacheSettingsInternal final : public LocalCacheSettingsInternal {
 public:
  explicit MemoryCacheSettingsInternal(const CoreMemorySettings& core_settings)
      : settings_(std::move(core_settings)) {}
  const CoreMemorySettings& core_settings() { return settings_; }
  void set_core_settings(const CoreMemorySettings& settings) {
    settings_ = settings;
  }

 private:
  CoreMemorySettings settings_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_LOCAL_CACHE_SETINGS_MAIN_H_
