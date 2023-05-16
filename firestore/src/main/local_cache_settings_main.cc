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

#include "firestore/src/main/local_cache_settings_main.h"
#include "firestore/src/include/firebase/firestore/local_cache_settings.h"

namespace firebase {
namespace firestore {

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::LocalCacheSettings() : impl_(std::make_shared<Impl>()) {}

LocalCacheSettings::LocalCacheSettings(
    PersistentCacheSettings persistent_cache_settings)
    : impl_(
          std::make_shared<Impl>(*std::move(persistent_cache_settings.impl_))) {
}

LocalCacheSettings::LocalCacheSettings(
    MemoryCacheSettings memory_cache_settings)
    : impl_(std::make_shared<Impl>(std::move(*memory_cache_settings.impl_))) {}

LocalCacheSettings::LocalCacheSettings(Impl impl)
    : impl_(std::make_shared<Impl>(std::move(impl))) {}

std::unique_ptr<LocalCacheSettings::PersistentCacheSettings>
LocalCacheSettings::persistent_cache_settings() const {
  if (!impl_->settings().has_value() ||
      !absl::holds_alternative<PersistentCacheSettings::Impl>(
          impl_->settings().value())) {
    return {};
  }
  return std::make_unique<LocalCacheSettings::PersistentCacheSettings>(
      LocalCacheSettings::PersistentCacheSettings(
          absl::get<PersistentCacheSettings::Impl>(impl_->settings().value())));
}

LocalCacheSettings LocalCacheSettings::WithCacheSettings(
    PersistentCacheSettings settings) const {
  return LocalCacheSettings(
      impl_->WithCacheSettings(std::move(*settings.impl_)));
}

std::unique_ptr<LocalCacheSettings::MemoryCacheSettings>
LocalCacheSettings::memory_cache_settings() const {
  if (!impl_->settings().has_value() ||
      !absl::holds_alternative<PersistentCacheSettings::Impl>(
          impl_->settings().value())) {
    return {};
  }
  return std::make_unique<LocalCacheSettings::MemoryCacheSettings>(
      LocalCacheSettings::MemoryCacheSettings(
          absl::get<MemoryCacheSettings::Impl>(impl_->settings().value())));
}

LocalCacheSettings LocalCacheSettings::WithCacheSettings(
    LocalCacheSettings::MemoryCacheSettings settings) const {
  return LocalCacheSettings(
      impl_->WithCacheSettings(std::move(*settings.impl_)));
}

void LocalCacheSettings::PrintTo(std::ostream& out) const { out << *impl_; }

bool operator==(const LocalCacheSettings& lhs, const LocalCacheSettings& rhs) {
  return *lhs.impl_ == *rhs.impl_;
}

bool LocalCacheSettings::Impl::operator==(const Impl& other) const {
  return settings_ == other.settings_;
}

void LocalCacheSettings::Impl::PrintTo(std::ostream& out) const {
  out << "LocalCacheSettings{";
  if (!settings_.has_value()) {
    out << "<unset>";
  } else if (absl::holds_alternative<MemoryCacheSettings::Impl>(*settings_)) {
    out << absl::get<MemoryCacheSettings::Impl>(*settings_);
  } else if (absl::holds_alternative<PersistentCacheSettings::Impl>(
                 *settings_)) {
    out << absl::get<PersistentCacheSettings::Impl>(*settings_);
  } else {
    FIRESTORE_UNREACHABLE();
  }
}

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings::PersistentCacheSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::PersistentCacheSettings::PersistentCacheSettings()
    : impl_(std::make_shared<Impl>()) {}

LocalCacheSettings::PersistentCacheSettings::PersistentCacheSettings(Impl impl)
    : impl_(std::make_shared<Impl>(std::move(impl))) {}

LocalCacheSettings::PersistentCacheSettings
LocalCacheSettings::PersistentCacheSettings::WithSizeBytes(
    int64_t size_bytes) const {
  return PersistentCacheSettings(impl_->WithSizeBytes(size_bytes));
}

int64_t LocalCacheSettings::PersistentCacheSettings::size_bytes() const {
  return impl_->size_bytes();
}

void LocalCacheSettings::PersistentCacheSettings::PrintTo(
    std::ostream& out) const {
  out << *impl_;
}

bool operator==(const LocalCacheSettings::PersistentCacheSettings& lhs,
                const LocalCacheSettings::PersistentCacheSettings& rhs) {
  return *lhs.impl_ == *rhs.impl_;
}

bool LocalCacheSettings::PersistentCacheSettings::Impl::operator==(
    const Impl& other) const {
  return settings_ == other.settings_;
}

void LocalCacheSettings::PersistentCacheSettings::Impl::PrintTo(
    std::ostream& out) const {
  out << "PersistentCacheSettings{";
  out << "size_bytes=" << settings_.size_bytes();
  out << "}";
}

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings::MemoryCacheSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::MemoryCacheSettings::MemoryCacheSettings()
    : impl_(std::make_shared<Impl>()) {}

LocalCacheSettings::MemoryCacheSettings::MemoryCacheSettings(
    LocalCacheSettings::MemoryCacheSettings::LruGCSettings lru_gc_settings)
    : impl_(std::make_shared<Impl>(std::move(*lru_gc_settings.impl_))) {}

LocalCacheSettings::MemoryCacheSettings::MemoryCacheSettings(
    LocalCacheSettings::MemoryCacheSettings::EagerGCSettings eager_gc_settings)
    : impl_(std::make_shared<Impl>(std::move(*eager_gc_settings.impl_))) {}

LocalCacheSettings::MemoryCacheSettings::MemoryCacheSettings(Impl impl)
    : impl_(std::make_shared<Impl>(std::move(impl))) {}

void LocalCacheSettings::MemoryCacheSettings::PrintTo(std::ostream& out) const {
  out << *impl_;
}

bool operator==(const LocalCacheSettings::MemoryCacheSettings& lhs,
                const LocalCacheSettings::MemoryCacheSettings& rhs) {
  return *lhs.impl_ == *rhs.impl_;
}

bool LocalCacheSettings::MemoryCacheSettings::Impl::operator==(
    const Impl& other) const {
  return settings_ == other.settings_;
}

void LocalCacheSettings::MemoryCacheSettings::Impl::PrintTo(
    std::ostream& out) const {
  out << "MemoryCacheSettings{";
  if (!settings_.has_value()) {
    out << "<unset>";
  } else if (absl::holds_alternative<EagerGCSettings::Impl>(*settings_)) {
    out << absl::get<EagerGCSettings::Impl>(*settings_);
  } else if (absl::holds_alternative<LruGCSettings::Impl>(*settings_)) {
    out << absl::get<LruGCSettings::Impl>(*settings_);
  } else {
    FIRESTORE_UNREACHABLE();
  }
  out << "}";
}

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings::MemoryCacheSettings::EagerGCSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::EagerGCSettings()
    : impl_(std::make_shared<Impl>()) {}

LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::EagerGCSettings(
    Impl impl)
    : impl_(std::make_shared<Impl>(std::move(impl))) {}

LocalCacheSettings::MemoryCacheSettings
LocalCacheSettings::MemoryCacheSettings::WithGarbageCollectorSettings(
    LruGCSettings settings) const {
  return MemoryCacheSettings(
      impl_->WithGarbageCollectorSettings(std::move(*settings.impl_)));
}

LocalCacheSettings::MemoryCacheSettings
LocalCacheSettings::MemoryCacheSettings::WithGarbageCollectorSettings(
    EagerGCSettings settings) const {
  return MemoryCacheSettings(
      impl_->WithGarbageCollectorSettings(std::move(*settings.impl_)));
}

void LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::PrintTo(
    std::ostream& out) const {
  out << *impl_;
}

bool operator==(
    const LocalCacheSettings::MemoryCacheSettings::EagerGCSettings& lhs,
    const LocalCacheSettings::MemoryCacheSettings::EagerGCSettings& rhs) {
  return *lhs.impl_ == *rhs.impl_;
}

bool LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::Impl::operator==(
    const Impl& other) const {
  return settings_ == other.settings_;
}

void LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::Impl::PrintTo(
    std::ostream& out) const {
  out << "EagerGCSettings{}";
}

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings::MemoryCacheSettings::LruGCSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::MemoryCacheSettings::LruGCSettings::LruGCSettings()
    : impl_(std::make_shared<Impl>()) {}

LocalCacheSettings::MemoryCacheSettings::LruGCSettings::LruGCSettings(Impl impl)
    : impl_(std::make_shared<Impl>(std::move(impl))) {}

LocalCacheSettings::MemoryCacheSettings::LruGCSettings
LocalCacheSettings::MemoryCacheSettings::LruGCSettings::WithSizeBytes(
    int64_t size_bytes) const {
  return LruGCSettings(impl_->WithSizeBytes(size_bytes));
}

int64_t LocalCacheSettings::MemoryCacheSettings::LruGCSettings::size_bytes()
    const {
  return impl_->size_bytes();
}

void LocalCacheSettings::MemoryCacheSettings::LruGCSettings::PrintTo(
    std::ostream& out) const {
  out << *impl_;
}

bool operator==(
    const LocalCacheSettings::MemoryCacheSettings::LruGCSettings& lhs,
    const LocalCacheSettings::MemoryCacheSettings::LruGCSettings& rhs) {
  return *lhs.impl_ == *rhs.impl_;
}

bool LocalCacheSettings::MemoryCacheSettings::LruGCSettings::Impl::operator==(
    const Impl& other) const {
  return settings_ == other.settings_;
}

void LocalCacheSettings::MemoryCacheSettings::LruGCSettings::Impl::PrintTo(
    std::ostream& out) const {
  out << "LruGCSettings{";
  out << "size_bytes=" << this->size_bytes();
  out << "}";
}

}  // namespace firestore
}  // namespace firebase
