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

namespace firebase {
namespace firestore {

using CoreMemorySettings = api::MemoryCacheSettings;
using CorePersistentSettings = api::PersistentCacheSettings;

class PersistentCacheSettingsInternal;
class MemoryCacheSettingsInternal;

class LocalCacheSettings {
  virtual ~LocalCacheSettings() = 0;
};

class PersistentCacheSettings : public LocalCacheSettings {
 public:
  static PersistentCacheSettings Create();

  PersistentCacheSettings WithSizeBytes(int64_t size) const;

 private:
  PersistentCacheSettings();
  PersistentCacheSettings(const PersistentCacheSettingsInternal& other);
  ~PersistentCacheSettings();

  std::unique_ptr<PersistentCacheSettingsInternal> settings_internal_;
};

class MemoryCacheSettings : public LocalCacheSettings {
 public:
  static MemoryCacheSettings Create();

 private:
  MemoryCacheSettings();
  MemoryCacheSettings(const MemoryCacheSettingsInternal& other);
  ~MemoryCacheSettings();

  std::unique_ptr<MemoryCacheSettingsInternal> settings_internal_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_
