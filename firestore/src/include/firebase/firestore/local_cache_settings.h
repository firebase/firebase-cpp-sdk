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
#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/main/local_cache_settings_main.h"

namespace firebase {
namespace firestore {

using CoreCacheSettings = api::LocalCacheSettings;
using CoreMemoryGarbageCollectorSettings = api::MemoryGargabeCollectorSettings;

class PersistentCacheSettingsInternal;
class MemoryCacheSettingsInternal;

class LocalCacheSettings {
 public:
  virtual ~LocalCacheSettings() = default;

 private:
  friend class FirestoreInternal;
  friend class Settings;

  virtual api::LocalCacheSettings::Kind kind() const = 0;
  virtual const CoreCacheSettings& core_cache_settings() const = 0;
};

class PersistentCacheSettings final : public LocalCacheSettings {
 public:
  static PersistentCacheSettings Create();

  PersistentCacheSettings(const PersistentCacheSettings& other);
  ~PersistentCacheSettings();

  PersistentCacheSettings WithSizeBytes(int64_t size) const;

 private:
  friend class Settings;

  PersistentCacheSettings();

  api::LocalCacheSettings::Kind kind() const override {
    return api::LocalCacheSettings::Kind::kPersistent;
  }

  const CoreCacheSettings& core_cache_settings() const override;

  std::unique_ptr<PersistentCacheSettingsInternal> settings_internal_;
};

class MemoryGarbageCollectorSettings;

class MemoryCacheSettings final : public LocalCacheSettings {
 public:
  static MemoryCacheSettings Create();
  MemoryCacheSettings(const MemoryCacheSettings& other);
  ~MemoryCacheSettings();

  MemoryCacheSettings WithGarbageCollectorSettings(
      const MemoryGarbageCollectorSettings& settings) const;

 private:
  friend class Settings;

  MemoryCacheSettings();

  api::LocalCacheSettings::Kind kind() const override {
    return api::LocalCacheSettings::Kind::kMemory;
  }

  const CoreCacheSettings& core_cache_settings() const override;

  std::unique_ptr<MemoryCacheSettingsInternal> settings_internal_;
};

class MemoryGarbageCollectorSettings {
 public:
  virtual ~MemoryGarbageCollectorSettings() = default;

 private:
  friend class MemoryCacheSettings;
  virtual const CoreMemoryGarbageCollectorSettings& core_gc_settings()
      const = 0;
};

class MemoryEagerGCSettings final : public MemoryGarbageCollectorSettings {
 public:
  static MemoryEagerGCSettings Create();
  ~MemoryEagerGCSettings();

 private:
  friend class MemoryCacheSettings;
  MemoryEagerGCSettings();

  const CoreMemoryGarbageCollectorSettings& core_gc_settings() const override {
    return settings_internal_->core_settings();
  }

  std::unique_ptr<MemoryEagerGCSettingsInternal> settings_internal_;
};

class MemoryLruGCSettings final : public MemoryGarbageCollectorSettings {
 public:
  static MemoryLruGCSettings Create();
  ~MemoryLruGCSettings();
  MemoryLruGCSettings WithSizeBytes(int64_t size);

 private:
  friend class MemoryCacheSettings;
  MemoryLruGCSettings();
  MemoryLruGCSettings(const MemoryLruGCSettingsInternal& other);

  const CoreMemoryGarbageCollectorSettings& core_gc_settings() const override {
    return settings_internal_->core_settings();
  }

  std::unique_ptr<MemoryLruGCSettingsInternal> settings_internal_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_
