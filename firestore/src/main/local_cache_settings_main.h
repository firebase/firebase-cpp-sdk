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

#include <memory>
#include <type_traits>

#include "firestore/src/include/firebase/firestore/local_cache_settings.h"
#include "firestore/src/common/macros.h"
#include "Firestore/core/src/api/settings.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"

namespace firebase {
namespace firestore {

class LocalCacheSettings::MemoryCacheSettings::LruGCSettings::Impl final {
 public:
  Impl() = default;

  explicit Impl(api::MemoryLruGcSettings settings) : settings_(std::move(settings)) {
  }

  const api::MemoryLruGcSettings& core_settings() const {
    return settings_;
  }

  int64_t size_bytes() const {
    return settings_.size_bytes();
  }

  Impl WithSizeBytes(int64_t size_bytes) {
    return Impl(settings_.WithSizeBytes(size_bytes));
  }

  bool operator==(const Impl&) const;
  bool operator!=(const Impl& rhs) const {
    return !(*this == rhs);
  }

 private:
  api::MemoryLruGcSettings settings_;
};

class LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::Impl final {
 public:
  Impl() = default;

  explicit Impl(api::MemoryEagerGcSettings settings) : settings_(std::move(settings)) {
  }

  const api::MemoryEagerGcSettings& core_settings() const {
    return settings_;
  }

  bool operator==(const Impl&) const;
  bool operator!=(const Impl& rhs) const {
    return !(*this == rhs);
  }

 private:
  api::MemoryEagerGcSettings settings_;
};

class LocalCacheSettings::MemoryCacheSettings::Impl final {
 public:
  Impl() = default;

  explicit Impl(LruGCSettings::Impl impl) : settings_(std::move(impl)) {
  }

  explicit Impl(EagerGCSettings::Impl impl) : settings_(std::move(impl)) {
  }

  std::unique_ptr<api::MemoryCacheSettings> ToCoreSettings() const {
    if (! settings_.has_value()) {
      return {};
    }
    if (absl::holds_alternative<EagerGCSettings::Impl>(*settings_)) {
      return std::make_unique<api::MemoryCacheSettings>(api::MemoryCacheSettings().WithMemoryGarbageCollectorSettings(absl::get<EagerGCSettings::Impl>(*settings_).core_settings()));
    }
    if (absl::holds_alternative<LruGCSettings::Impl>(*settings_)) {
      return std::make_unique<api::MemoryCacheSettings>(api::MemoryCacheSettings().WithMemoryGarbageCollectorSettings(absl::get<LruGCSettings::Impl>(*settings_).core_settings()));
    }
    FIRESTORE_UNREACHABLE();
  }

  Impl WithGarbageCollectorSettings(LruGCSettings::Impl settings) const {
    return Impl(std::move(settings));
  }

  Impl WithGarbageCollectorSettings(EagerGCSettings::Impl settings) const {
    return Impl(std::move(settings));
  }

  bool operator==(const Impl&) const;
  bool operator!=(const Impl& rhs) const {
    return !(*this == rhs);
  }

 private:
  absl::optional<absl::variant<EagerGCSettings::Impl, LruGCSettings::Impl>> settings_;
};

class LocalCacheSettings::PersistentCacheSettings::Impl final {
 public:
  Impl() = default;

  explicit Impl(api::PersistentCacheSettings settings) : settings_(std::move(settings)) {
  }

  std::unique_ptr<api::PersistentCacheSettings> ToCoreSettings() const {
    return std::make_unique<api::PersistentCacheSettings>(settings_);
  }

  int64_t size_bytes() const {
    return settings_.size_bytes();
  }

  Impl WithSizeBytes(int64_t size_bytes) {
    return Impl(settings_.WithSizeBytes(size_bytes));
  }

  bool operator==(const Impl&) const;
  bool operator!=(const Impl& rhs) const {
    return !(*this == rhs);
  }

 private:
  api::PersistentCacheSettings settings_;
};

class LocalCacheSettings::Impl final {
 public:
  Impl() = default;

  explicit Impl(MemoryCacheSettings::Impl impl) : settings_(std::move(impl)) {
  }
  explicit Impl(PersistentCacheSettings::Impl impl) : settings_(std::move(impl)) {
  }

  std::unique_ptr<api::LocalCacheSettings> ToCoreSettings() const {
    if (! settings_.has_value()) {
      return {};
    }
    if (absl::holds_alternative<MemoryCacheSettings::Impl>(*settings_)) {
      absl::get<MemoryCacheSettings::Impl>(*settings_).ToCoreSettings();
    }
    if (absl::holds_alternative<PersistentCacheSettings::Impl>(*settings_)) {
      absl::get<PersistentCacheSettings::Impl>(*settings_).ToCoreSettings();
    }
    FIRESTORE_UNREACHABLE();
  }

  Impl WithCacheSettings(MemoryCacheSettings::Impl settings) const {
    return Impl(std::move(settings));
  }

  Impl WithCacheSettings(PersistentCacheSettings::Impl settings) const {
    return Impl(std::move(settings));
  }

  const absl::optional<absl::variant<MemoryCacheSettings::Impl, PersistentCacheSettings::Impl>>& settings() const {
    return settings_;
  }

  bool operator==(const Impl&) const;
  bool operator!=(const Impl& rhs) const {
    return !(*this == rhs);
  }

 private:
  absl::optional<absl::variant<MemoryCacheSettings::Impl, PersistentCacheSettings::Impl>> settings_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_LOCAL_CACHE_SETTINGS_MAIN_H_
