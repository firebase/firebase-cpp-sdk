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

#ifndef FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_
#define FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_

#include <cstdint>
#include <memory>

#include "Firestore/core/src/api/settings.h"
#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/main/firestore_main.h"
#include "firestore/src/main/local_cache_settings_main.h"

namespace firebase {
namespace firestore {

namespace {
using CoreCacheSettings = api::LocalCacheSettings;
using CoreMemoryGarbageCollectorSettings = api::MemoryGargabeCollectorSettings;
}  // namespace

class PersistentCacheSettingsInternal;
class MemoryCacheSettingsInternal;

/**
 * Abstract class implemented by all supported cache settings.
 *
 * `PersistentCacheSettings` and `MemoryCacheSettings` are the only cache types
 * supported by the SDK. Custom implementation is not supported.
 */
class LocalCacheSettings {
 public:
  virtual ~LocalCacheSettings() = default;

 private:
  friend class FirestoreInternal;
  friend class Settings;

  virtual api::LocalCacheSettings::Kind kind() const = 0;
  virtual const CoreCacheSettings& core_cache_settings() const = 0;
};

/**
 * Configures the SDK to use a persistent cache. Firestore documents and
 * mutations are persisted across App restart.
 *
 * This is the default cache type unless explicitly specified otherwise.
 *
 * To use, create an instance using `PersistentCacheSettings::Create`, then
 * pass it to an instance of `Settings` via `set_local_cache_settings()`, and
 * use the `Settings` instance to configure the Firestore SDK.
 */
class PersistentCacheSettings final : public LocalCacheSettings {
 public:
  /** Create a default instance `PersistenceCacheSettings`. */
  static PersistentCacheSettings Create();

  /** Copy constructor. */
  PersistentCacheSettings(const PersistentCacheSettings& other);

  /** Copy assignment. */
  PersistentCacheSettings& operator=(const PersistentCacheSettings& other);

  ~PersistentCacheSettings();

  friend bool operator==(const PersistentCacheSettings& lhs,
                         const PersistentCacheSettings& rhs);

  /**
   * Copies this settings instance, with the approximate cache size threshold
   * for the on-disk data set to the given number in term of number of bytes,
   * and return the new setting instance.
   *
   * If the cache grows beyond this size, Firestore SDK will start removing data
   * that hasn't been recently used. The SDK does not guarantee that the cache
   * will stay below that size, only that if the cache exceeds the given size,
   * cleanup will be attempted.
   *
   * By default, persistence cache is enabled with a cache size of 100 MB. The
   * minimum value is 1 MB.
   */
  PersistentCacheSettings WithSizeBytes(int64_t size) const;

  /**
   * Returns the approximate cache size threshold configured. Garbage collection
   * kicks in once the cache size exceeds this threshold.
   */
  int64_t size_bytes() const {
    return settings_internal_->core_settings().size_bytes();
  }

 private:
  friend class Settings;
  friend class FirestoreInternal;

  static PersistentCacheSettings CreateFromCoreSettings(
      const CorePersistentSettings& core_settings);
  PersistentCacheSettings();

  api::LocalCacheSettings::Kind kind() const override {
    return api::LocalCacheSettings::Kind::kPersistent;
  }

  // Get the corresponding settings object from the core sdk.
  const CoreCacheSettings& core_cache_settings() const override;

  std::unique_ptr<PersistentCacheSettingsInternal> settings_internal_;
};

class MemoryGarbageCollectorSettings;

/**
 * Configures the SDK to use a memory cache. Firestore documents and mutations
 * are NOT persisted across App restart.
 *
 * To use, create an instance using `MemoryCacheSettings::Create`, then
 * pass it to an instance of `Settings` via `set_local_cache_settings()`, and
 * use the `Settings` instance to configure the Firestore SDK.
 */
class MemoryCacheSettings final : public LocalCacheSettings {
 public:
  /** Create a default instance `MemoryCacheSettings`. */
  static MemoryCacheSettings Create();

  /** Copy constructor. */
  MemoryCacheSettings(const MemoryCacheSettings& other);

  /** Copy assignment. */
  MemoryCacheSettings& operator=(const MemoryCacheSettings& other);

  ~MemoryCacheSettings();

  friend bool operator==(const MemoryCacheSettings& lhs,
                         const MemoryCacheSettings& rhs);

  /**
   * Copies this settings instance, with its `MemoryGarbageCollectorSettins` set
   * the the given parameter, and returns the new settings instance.
   */
  MemoryCacheSettings WithGarbageCollectorSettings(
      const MemoryGarbageCollectorSettings& settings) const;

 private:
  friend class Settings;
  friend class FirestoreInternal;

  static MemoryCacheSettings CreateFromCoreSettings(
      const CoreMemorySettings& core_settings);
  MemoryCacheSettings();

  api::LocalCacheSettings::Kind kind() const override {
    return api::LocalCacheSettings::Kind::kMemory;
  }

  // Get the corresponding settings object from the core sdk.
  const CoreCacheSettings& core_cache_settings() const override;

  std::unique_ptr<MemoryCacheSettingsInternal> settings_internal_;
};

/**
 * Abstract class implemented by all supported memory garbage collector.
 *
 * `MemoryEagerGCSettings` and `MemoryLruGCSettings` are the only memory
 * garbage collectors supported by the SDK. Custom implementation is not
 * supported.
 */
class MemoryGarbageCollectorSettings {
 public:
  virtual ~MemoryGarbageCollectorSettings() = default;

 private:
  friend class MemoryCacheSettings;
  virtual const CoreMemoryGarbageCollectorSettings& core_gc_settings()
      const = 0;
};

/**
 * Configures the memory cache to use a garbage collector with an eager
 * strategy.
 *
 * An eager garbage collector deletes documents whenever they are not part of
 * any active queries, and have no local mutations attached to them.
 *
 * This collector tries to ensure lowest memory footprints from the SDK,
 * at the risk of documents not being cached for offline queries or for
 * direct queries to the cache.
 *
 * To use, pass an instance of `MemoryEagerGCSettings` to
 * `MemoryCacheSettings::WithGarbageCollectorSettings()` to get a new instance
 * of `MemoryCacheSettings`, which can be used to configure the SDK.
 */
class MemoryEagerGCSettings final : public MemoryGarbageCollectorSettings {
 public:
  /** Create a default instance `MemoryEagerGCSettings`. */
  static MemoryEagerGCSettings Create();

  ~MemoryEagerGCSettings();

  friend bool operator==(const MemoryEagerGCSettings& lhs,
                         const MemoryEagerGCSettings& rhs);

 private:
  friend class MemoryCacheSettings;
  MemoryEagerGCSettings();

  const CoreMemoryGarbageCollectorSettings& core_gc_settings() const override {
    return settings_internal_->core_settings();
  }

  std::unique_ptr<MemoryEagerGCSettingsInternal> settings_internal_;
};

/**
 * Configures the memory cache to use a garbage collector with an
 * least-recently-used strategy.
 *
 * A LRU garbage collector deletes Least-Recently-Used documents in multiple
 * batches.
 *
 * This collector is configured with a target size, and will only perform
 * collection when the cached documents exceed the target size. It avoids
 * querying backend repeated for the same query or document, at the risk
 * of having a larger memory footprint.
 *
 * To use, pass an instance of `MemoryLRUGCSettings` to
 * `MemoryCacheSettings::WithGarbageCollectorSettings()` to get a new instance
 * of `MemoryCacheSettings`, which can be used to configure the SDK.
 */
class MemoryLruGCSettings final : public MemoryGarbageCollectorSettings {
 public:
  /** Create a default instance `MemoryLruGCSettings`. */
  static MemoryLruGCSettings Create();
  ~MemoryLruGCSettings();

  friend bool operator==(const MemoryLruGCSettings& lhs,
                         const MemoryLruGCSettings& rhs);

  /**
   * Copies this settings instance, with the approximate cache size threshold
   * for the memory data set to the given number in term of number of bytes, and
   * return the new setting instance.
   *
   * If the cache grows beyond this size, Firestore SDK will start removing data
   * that hasn't been recently used. The SDK does not guarantee that the cache
   * will stay below that size, only that if the cache exceeds the given size,
   * cleanup will be attempted.
   *
   * By default, memory LRU cache is enabled with a cache size of 100 MB. The
   * minimum value is 1 MB.
   */
  MemoryLruGCSettings WithSizeBytes(int64_t size);

  /**
   * Returns the approximate cache size threshold configured. Garbage collection
   * kicks in once the cache size exceeds this threshold.
   */
  int64_t size_bytes() const {
    return settings_internal_->core_settings().size_bytes();
  }

 private:
  friend class MemoryCacheSettings;
  MemoryLruGCSettings();
  MemoryLruGCSettings(const MemoryLruGCSettingsInternal& other);

  const CoreMemoryGarbageCollectorSettings& core_gc_settings() const override {
    return settings_internal_->core_settings();
  }

  std::unique_ptr<MemoryLruGCSettingsInternal> settings_internal_;
};

bool operator==(const MemoryCacheSettings& lhs, const MemoryCacheSettings& rhs);

bool operator!=(const MemoryCacheSettings& lhs, const MemoryCacheSettings& rhs);

bool operator==(const PersistentCacheSettings& lhs,
                const PersistentCacheSettings& rhs);

bool operator!=(const PersistentCacheSettings& lhs,
                const PersistentCacheSettings& rhs);

bool operator==(const MemoryEagerGCSettings& lhs,
                const MemoryEagerGCSettings& rhs);

bool operator!=(const MemoryEagerGCSettings& lhs,
                const MemoryEagerGCSettings& rhs);

bool operator==(const MemoryLruGCSettings& lhs, const MemoryLruGCSettings& rhs);

bool operator!=(const MemoryLruGCSettings& lhs, const MemoryLruGCSettings& rhs);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_
