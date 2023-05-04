/*
 * Copyright 2018 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_
#define FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_

#include <cstdint>
#include <memory>

#include "Firestore/core/src/api/settings.h"
#include "firestore/src/main/firestore_main.h"

namespace firebase {
namespace firestore {

using CoreCacheSettings = api::LocalCacheSettings;
using CoreMemorySettings = api::MemoryCacheSettings;
using CorePersistentSettings = api::PersistentCacheSettings;

class Settings;
class PersistentCacheSettingsInternal;
class MemoryCacheSettingsInternal;

class LocalCacheSettings {
 public:
  virtual api::LocalCacheSettings::Kind kind() const = 0;
  virtual ~LocalCacheSettings() = default;

 private:
  friend class FirestoreInternal;

  virtual const CoreCacheSettings& core_cache_settings() const = 0;
};

class PersistentCacheSettings final : public LocalCacheSettings {
 public:
  static PersistentCacheSettings Create();
  ~PersistentCacheSettings();
  PersistentCacheSettings(const PersistentCacheSettingsInternal& other);

  PersistentCacheSettings WithSizeBytes(int64_t size) const;
  const CoreCacheSettings& core_cache_settings() const override;
  api::LocalCacheSettings::Kind kind() const override {
    return api::LocalCacheSettings::Kind::kPersistent;
  }

 private:
  friend class Settings;

  PersistentCacheSettings();

  std::unique_ptr<PersistentCacheSettingsInternal> settings_internal_;
};

class MemoryCacheSettings final : public LocalCacheSettings {
 public:
  static MemoryCacheSettings Create();
  ~MemoryCacheSettings();

  const CoreCacheSettings& core_cache_settings() const override;
  api::LocalCacheSettings::Kind kind() const override {
    return api::LocalCacheSettings::Kind::kMemory;
  }

  MemoryCacheSettings(const MemoryCacheSettingsInternal& other);

 private:
  friend class Settings;

  MemoryCacheSettings();

  std::unique_ptr<MemoryCacheSettingsInternal> settings_internal_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_
