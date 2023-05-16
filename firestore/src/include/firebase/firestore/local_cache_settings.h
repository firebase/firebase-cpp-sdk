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
#include <iostream>
#include <memory>
#include <sstream>

namespace firebase {
namespace firestore {

class LocalCacheSettings final {
 public:
  LocalCacheSettings();

  LocalCacheSettings(const LocalCacheSettings&) = default;
  LocalCacheSettings& operator=(const LocalCacheSettings&) = default;
  LocalCacheSettings(LocalCacheSettings&&) = default;
  LocalCacheSettings& operator=(LocalCacheSettings&&) = default;

  class PersistentCacheSettings;
  std::unique_ptr<PersistentCacheSettings> persistent_cache_settings() const;
  LocalCacheSettings WithCacheSettings(const PersistentCacheSettings&) const;

  class MemoryCacheSettings;
  std::unique_ptr<MemoryCacheSettings> memory_cache_settings() const;
  LocalCacheSettings WithCacheSettings(const MemoryCacheSettings&) const;

  friend bool operator==(const LocalCacheSettings&, const LocalCacheSettings&);

  void PrintTo(std::ostream& out) const;

  friend std::ostream& operator<<(std::ostream& out,
                                  const LocalCacheSettings& self) {
    self.PrintTo(out);
    return out;
  }

  std::string ToString() const { return (std::ostringstream() << *this).str(); }

 private:
  friend class FirestoreInternal;

  class Impl;

  explicit LocalCacheSettings(Impl);

  std::shared_ptr<Impl> impl_;
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
class LocalCacheSettings::PersistentCacheSettings final {
 public:
  PersistentCacheSettings();

  PersistentCacheSettings(const PersistentCacheSettings&) = default;
  PersistentCacheSettings& operator=(const PersistentCacheSettings&) = default;
  PersistentCacheSettings(PersistentCacheSettings&&) = default;
  PersistentCacheSettings& operator=(PersistentCacheSettings&&) = default;

  /** Equality function. */
  friend bool operator==(const PersistentCacheSettings&,
                         const PersistentCacheSettings&);

  void PrintTo(std::ostream& out) const;

  friend std::ostream& operator<<(std::ostream& out,
                                  const PersistentCacheSettings& self) {
    self.PrintTo(out);
    return out;
  }

  std::string ToString() const { return (std::ostringstream() << *this).str(); }

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
  PersistentCacheSettings WithSizeBytes(int64_t size_bytes) const;

  /**
   * Returns the approximate cache size threshold configured. Garbage collection
   * kicks in once the cache size exceeds this threshold.
   */
  int64_t size_bytes() const;

 private:
  friend class LocalCacheSettings;

  class Impl;

  explicit PersistentCacheSettings(Impl);

  std::shared_ptr<Impl> impl_;
};

/**
 * Configures the SDK to use a memory cache. Firestore documents and mutations
 * are NOT persisted across App restart.
 *
 * To use, create an instance using `MemoryCacheSettings::Create`, then
 * pass it to an instance of `Settings` via `set_local_cache_settings()`, and
 * use the `Settings` instance to configure the Firestore SDK.
 */
class LocalCacheSettings::MemoryCacheSettings final {
 public:
  MemoryCacheSettings();

  MemoryCacheSettings(const MemoryCacheSettings&) = default;
  MemoryCacheSettings& operator=(const MemoryCacheSettings&) = default;
  MemoryCacheSettings(MemoryCacheSettings&&) = default;
  MemoryCacheSettings& operator=(MemoryCacheSettings&&) = default;

  /** Equality function. */
  friend bool operator==(const MemoryCacheSettings&,
                         const MemoryCacheSettings&);

  void PrintTo(std::ostream& out) const;

  friend std::ostream& operator<<(std::ostream& out,
                                  const MemoryCacheSettings& self) {
    self.PrintTo(out);
    return out;
  }

  std::string ToString() const { return (std::ostringstream() << *this).str(); }

  class LruGCSettings;
  class EagerGCSettings;

  /**
   * Copies this settings instance, with its `MemoryGarbageCollectorSettings`
   * set the the given parameter, and returns the new settings instance.
   */
  MemoryCacheSettings WithGarbageCollectorSettings(const LruGCSettings&) const;

  /**
   * Copies this settings instance, with its `MemoryGarbageCollectorSettings`
   * set the the given parameter, and returns the new settings instance.
   */
  MemoryCacheSettings WithGarbageCollectorSettings(
      const EagerGCSettings&) const;

 private:
  friend class LocalCacheSettings;

  class Impl;

  explicit MemoryCacheSettings(Impl);

  std::shared_ptr<Impl> impl_;
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
 * To use, pass an instance of `EagerGCSettings` to
 * `MemoryCacheSettings::WithGarbageCollectorSettings()` to get a new instance
 * of `MemoryCacheSettings`, which can be used to configure the SDK.
 */
class LocalCacheSettings::MemoryCacheSettings::EagerGCSettings final {
 public:
  EagerGCSettings();

  EagerGCSettings(const EagerGCSettings&) = default;
  EagerGCSettings& operator=(const EagerGCSettings&) = default;
  EagerGCSettings(EagerGCSettings&&) = default;
  EagerGCSettings& operator=(EagerGCSettings&&) = default;

  /** Equality function. */
  friend bool operator==(const EagerGCSettings&, const EagerGCSettings&);

  void PrintTo(std::ostream& out) const;

  friend std::ostream& operator<<(std::ostream& out,
                                  const EagerGCSettings& self) {
    self.PrintTo(out);
    return out;
  }

  std::string ToString() const { return (std::ostringstream() << *this).str(); }

 private:
  friend class LocalCacheSettings::MemoryCacheSettings;

  class Impl;

  explicit EagerGCSettings(Impl);

  std::shared_ptr<Impl> impl_;
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
class LocalCacheSettings::MemoryCacheSettings::LruGCSettings final {
 public:
  LruGCSettings();

  LruGCSettings(const LruGCSettings&) = default;
  LruGCSettings& operator=(const LruGCSettings&) = default;
  LruGCSettings(LruGCSettings&&) = default;
  LruGCSettings& operator=(LruGCSettings&&) = default;

  /** Equality function. */
  friend bool operator==(const LruGCSettings&, const LruGCSettings&);

  void PrintTo(std::ostream& out) const;

  friend std::ostream& operator<<(std::ostream& out,
                                  const LruGCSettings& self) {
    self.PrintTo(out);
    return out;
  }

  std::string ToString() const { return (std::ostringstream() << *this).str(); }

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
  LruGCSettings WithSizeBytes(int64_t size) const;

  /**
   * Returns the approximate cache size threshold configured. Garbage collection
   * kicks in once the cache size exceeds this threshold.
   */
  int64_t size_bytes() const;

 private:
  friend class LocalCacheSettings::MemoryCacheSettings;

  class Impl;

  explicit LruGCSettings(Impl);

  std::shared_ptr<Impl> impl_;
};

/** Inequality function. */
inline bool operator!=(const LocalCacheSettings& lhs,
                       const LocalCacheSettings& rhs) {
  return !(lhs == rhs);
}

/** Inequality function. */
inline bool operator!=(const LocalCacheSettings::MemoryCacheSettings& lhs,
                       const LocalCacheSettings::MemoryCacheSettings& rhs) {
  return !(lhs == rhs);
}

/** Inequality function. */
inline bool operator!=(const LocalCacheSettings::PersistentCacheSettings& lhs,
                       const LocalCacheSettings::PersistentCacheSettings& rhs) {
  return !(lhs == rhs);
}

/** Inequality function. */
inline bool operator!=(
    const LocalCacheSettings::MemoryCacheSettings::EagerGCSettings& lhs,
    const LocalCacheSettings::MemoryCacheSettings::EagerGCSettings& rhs) {
  return !(lhs == rhs);
}

/** Inequality function. */
inline bool operator!=(
    const LocalCacheSettings::MemoryCacheSettings::LruGCSettings& lhs,
    const LocalCacheSettings::MemoryCacheSettings::LruGCSettings& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_LOCAL_CACHE_SETTINGS_H_
