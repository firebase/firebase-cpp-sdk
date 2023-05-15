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
#include "firestore/src/main/local_cache_settings_main.h"

namespace firebase {
namespace firestore {

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::LocalCacheSettings() : impl_(std::make_shared<Impl>()) {
}

LocalCacheSettings::LocalCacheSettings(Impl impl) : impl_(std::make_shared<Impl>(std::move(impl))) {
}

std::unique_ptr<LocalCacheSettings::PersistentCacheSettings> LocalCacheSettings::persistent_cache_settings() const {
  if (!impl_->settings().has_value() || !absl::holds_alternative<PersistentCacheSettings::Impl>(impl_->settings().value())) {
    return {};
  }
  return std::make_unique<LocalCacheSettings::PersistentCacheSettings>(LocalCacheSettings::PersistentCacheSettings(absl::get<PersistentCacheSettings::Impl>(impl_->settings().value())));
}

LocalCacheSettings LocalCacheSettings::WithCacheSettings(const PersistentCacheSettings& settings) const {
  return LocalCacheSettings(impl_->WithCacheSettings(*settings.impl_));
}

std::unique_ptr<LocalCacheSettings::MemoryCacheSettings> LocalCacheSettings::memory_cache_settings() const {
  if (!impl_->settings().has_value() || !absl::holds_alternative<PersistentCacheSettings::Impl>(impl_->settings().value())) {
    return {};
  }
  return std::make_unique<LocalCacheSettings::MemoryCacheSettings>(LocalCacheSettings::MemoryCacheSettings(absl::get<MemoryCacheSettings::Impl>(impl_->settings().value())));
}

LocalCacheSettings LocalCacheSettings::WithCacheSettings(const LocalCacheSettings::MemoryCacheSettings& settings) const {
  return LocalCacheSettings(impl_->WithCacheSettings(*settings.impl_));
}

bool operator==(const LocalCacheSettings& lhs, const LocalCacheSettings& rhs) {
  return lhs.impl_ == rhs.impl_;
}

bool LocalCacheSettings::Impl::operator==(const Impl& other) const {
  return settings_ == other.settings_;
}

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings::PersistentCacheSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::PersistentCacheSettings::PersistentCacheSettings() : impl_(std::make_shared<Impl>()) {
}

LocalCacheSettings::PersistentCacheSettings::PersistentCacheSettings(Impl impl) : impl_(std::make_shared<Impl>(std::move(impl))) {
}

LocalCacheSettings::PersistentCacheSettings LocalCacheSettings::PersistentCacheSettings::WithSizeBytes(int64_t size_bytes) const {
  return PersistentCacheSettings(impl_->WithSizeBytes(size_bytes));
}

int64_t LocalCacheSettings::PersistentCacheSettings::size_bytes() const {
  return impl_->size_bytes();
}

bool operator==(const LocalCacheSettings::PersistentCacheSettings& lhs, const LocalCacheSettings::PersistentCacheSettings& rhs) {
  return lhs.impl_ == rhs.impl_;
}

bool LocalCacheSettings::PersistentCacheSettings::Impl::operator==(const Impl& other) const {
  return settings_ == other.settings_;
}

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings::MemoryCacheSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::MemoryCacheSettings::MemoryCacheSettings() : impl_(std::make_shared<Impl>()) {
}

LocalCacheSettings::MemoryCacheSettings::MemoryCacheSettings(Impl impl) : impl_(std::make_shared<Impl>(std::move(impl))) {
}

bool operator==(const LocalCacheSettings::MemoryCacheSettings& lhs, const LocalCacheSettings::MemoryCacheSettings& rhs) {
  return lhs.impl_ == rhs.impl_;
}

bool LocalCacheSettings::MemoryCacheSettings::Impl::operator==(const Impl& other) const {
  return settings_ == other.settings_;
}

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings::MemoryCacheSettings::EagerGCSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::EagerGCSettings() : impl_(std::make_shared<Impl>()) {
}

LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::EagerGCSettings(Impl impl) : impl_(std::make_shared<Impl>(std::move(impl))) {
}

LocalCacheSettings::MemoryCacheSettings LocalCacheSettings::MemoryCacheSettings::WithGarbageCollectorSettings(const LruGCSettings& settings) const {
  return MemoryCacheSettings(impl_->WithGarbageCollectorSettings(*settings.impl_));
}

LocalCacheSettings::MemoryCacheSettings LocalCacheSettings::MemoryCacheSettings::WithGarbageCollectorSettings(const EagerGCSettings& settings) const {
  return MemoryCacheSettings(impl_->WithGarbageCollectorSettings(*settings.impl_));
}

bool operator==(const LocalCacheSettings::MemoryCacheSettings::EagerGCSettings& lhs, const LocalCacheSettings::MemoryCacheSettings::EagerGCSettings& rhs) {
  return lhs.impl_ == rhs.impl_;
}

bool LocalCacheSettings::MemoryCacheSettings::EagerGCSettings::Impl::operator==(const Impl& other) const {
  return settings_ == other.settings_;
}

////////////////////////////////////////////////////////////////////////////////
// class LocalCacheSettings::MemoryCacheSettings::LruGCSettings
////////////////////////////////////////////////////////////////////////////////

LocalCacheSettings::MemoryCacheSettings::LruGCSettings::LruGCSettings() : impl_(std::make_shared<Impl>()) {
}

LocalCacheSettings::MemoryCacheSettings::LruGCSettings::LruGCSettings(Impl impl) : impl_(std::make_shared<Impl>(std::move(impl))) {
}

LocalCacheSettings::MemoryCacheSettings::LruGCSettings LocalCacheSettings::MemoryCacheSettings::LruGCSettings::WithSizeBytes(int64_t size_bytes) const {
  return LruGCSettings(impl_->WithSizeBytes(size_bytes));
}

int64_t LocalCacheSettings::MemoryCacheSettings::LruGCSettings::size_bytes() const {
  return impl_->size_bytes();
}

bool operator==(const LocalCacheSettings::MemoryCacheSettings::LruGCSettings& lhs, const LocalCacheSettings::MemoryCacheSettings::LruGCSettings& rhs) {
  return lhs.impl_ == rhs.impl_;
}

bool LocalCacheSettings::MemoryCacheSettings::LruGCSettings::Impl::operator==(const Impl& other) const {
  return settings_ == other.settings_;
}

}  // namespace firestore
}  // namespace firebase
