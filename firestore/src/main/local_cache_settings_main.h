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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_LOCAL_CACHE_SETTINGS_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_LOCAL_CACHE_SETTINGS_MAIN_H_

#include <cstdint>
#include <utility>

#include "Firestore/core/src/api/settings.h"
#include "firebase/firestore/local_cache_settings.h"

namespace firebase {
namespace firestore {

class LocalCacheSettingsInternal {
 public:
  friend bool operator==(const LocalCacheSettingsInternal& lhs,
                         const LocalCacheSettingsInternal& rhs) {
    return &lhs == &rhs || lhs.core_settings() == rhs.core_settings();
  }

  virtual const api::LocalCacheSettings& core_settings() const = 0;
};

class PersistentCacheSettingsInternal final
    : public LocalCacheSettingsInternal {
 public:
  explicit PersistentCacheSettingsInternal() = default;

  friend bool operator==(const PersistentCacheSettingsInternal& lhs,
                         const PersistentCacheSettingsInternal& rhs) {
    return &lhs == &rhs || lhs.settings_ == rhs.settings_;
  }

  PersistentCacheSettingsInternal WithSizeBytes(int64_t size) {
    PersistentCacheSettingsInternal result;
    result.set_core_settings(this->core_settings().WithSizeBytes(size));
    return result;
  }

  int64_t size_bytes() const { return settings_.size_bytes(); }

  const api::PersistentCacheSettings& core_settings() const override {
    return settings_;
  }
  void set_core_settings(const api::PersistentCacheSettings& settings) {
    settings_ = settings;
  }

 private:
  api::PersistentCacheSettings settings_;
};

class MemoryGarbageCollectorSettingsInternal {
 public:
  friend bool operator==(const MemoryGarbageCollectorSettingsInternal& lhs,
                         const MemoryGarbageCollectorSettingsInternal& rhs) {
    return &lhs == &rhs || lhs.core_settings() == rhs.core_settings();
  }

  virtual const api::MemoryGargabeCollectorSettings& core_settings() const = 0;
};

class MemoryEagerGCSettingsInternal final
    : public MemoryGarbageCollectorSettingsInternal {
 public:
  explicit MemoryEagerGCSettingsInternal() = default;

  friend bool operator==(const MemoryEagerGCSettingsInternal& lhs,
                         const MemoryEagerGCSettingsInternal& rhs) {
    return &lhs == &rhs || lhs.settings_ == rhs.settings_;
  }

  const api::MemoryEagerGcSettings& core_settings() const override {
    return settings_;
  }
  void set_core_settings(const api::MemoryEagerGcSettings& settings) {
    settings_ = settings;
  }

 private:
  api::MemoryEagerGcSettings settings_;
};

class MemoryLruGCSettingsInternal final
    : public MemoryGarbageCollectorSettingsInternal {
 public:
  explicit MemoryLruGCSettingsInternal() = default;

  friend bool operator==(const MemoryLruGCSettingsInternal& lhs,
                         const MemoryLruGCSettingsInternal& rhs) {
    return &lhs == &rhs || lhs.settings_ == rhs.settings_;
  }

  MemoryLruGCSettingsInternal WithSizeBytes(int64_t size) {
    MemoryLruGCSettingsInternal result;
    result.set_core_settings(this->core_settings().WithSizeBytes(size));
    return result;
  }

  int64_t size_bytes() const { return settings_.size_bytes(); }

  const api::MemoryLruGcSettings& core_settings() const override {
    return settings_;
  }
  void set_core_settings(const api::MemoryLruGcSettings& settings) {
    settings_ = settings;
  }

 private:
  api::MemoryLruGcSettings settings_;
};

class MemoryCacheSettingsInternal final : public LocalCacheSettingsInternal {
 public:
  explicit MemoryCacheSettingsInternal() = default;

  friend bool operator==(const MemoryCacheSettingsInternal& lhs,
                         const MemoryCacheSettingsInternal& rhs) {
    return &lhs == &rhs || lhs.settings_ == rhs.settings_;
  }

  MemoryCacheSettingsInternal WithGarbageCollectorSettings(
      const MemoryGarbageCollectorSettings& gc_settings) {
    return MemoryCacheSettingsInternal(
        api::MemoryCacheSettings().WithMemoryGarbageCollectorSettings(
            gc_settings.internal().core_settings()));
  }

  const api::MemoryCacheSettings& core_settings() const override {
    return settings_;
  }

 private:
  explicit MemoryCacheSettingsInternal(const api::MemoryCacheSettings& settings)
      : settings_(settings) {}
  api::MemoryCacheSettings settings_;
};

inline bool operator!=(const MemoryCacheSettingsInternal& lhs,
                       const MemoryCacheSettingsInternal& rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const MemoryLruGCSettingsInternal& lhs,
                       const MemoryLruGCSettingsInternal& rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const MemoryEagerGCSettingsInternal& lhs,
                       const MemoryEagerGCSettingsInternal& rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const PersistentCacheSettingsInternal& lhs,
                       const PersistentCacheSettingsInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_LOCAL_CACHE_SETTINGS_MAIN_H_
