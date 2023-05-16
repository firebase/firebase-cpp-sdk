/*
 * Copyright 2021 Google LLC
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

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "firebase/firestore.h"
#include "firebase/firestore/local_cache_settings.h"
#include "firebase_test_framework.h"

#include "gtest/gtest.h"

#if !defined(__ANDROID__)
#include "Firestore/core/src/util/warnings.h"
#else
#endif  // !defined(__ANDROID__)

namespace firebase {
namespace firestore {
namespace {

TEST(SettingsTest, Equality) {
  constexpr int64_t kFiveMb = 5 * 1024 * 1024;
  constexpr int64_t kSixMb = 6 * 1024 * 1024;

  Settings settings1;
  settings1.set_host("foo");
  settings1.set_ssl_enabled(true);
  SUPPRESS_DEPRECATED_DECLARATIONS_BEGIN()
  settings1.set_persistence_enabled(true);
  settings1.set_cache_size_bytes(kFiveMb);
  SUPPRESS_END()

  Settings settings2;
  settings2.set_host("bar");
  settings2.set_ssl_enabled(true);
  SUPPRESS_DEPRECATED_DECLARATIONS_BEGIN()
  settings2.set_persistence_enabled(true);
  settings2.set_cache_size_bytes(kFiveMb);
  SUPPRESS_END()

  Settings settings3;
  settings3.set_host("foo");
  settings3.set_ssl_enabled(false);
  SUPPRESS_DEPRECATED_DECLARATIONS_BEGIN()
  settings3.set_persistence_enabled(true);
  settings3.set_cache_size_bytes(kFiveMb);
  SUPPRESS_END()

  Settings settings4;
  settings4.set_host("foo");
  settings4.set_ssl_enabled(true);
  SUPPRESS_DEPRECATED_DECLARATIONS_BEGIN()
  settings4.set_persistence_enabled(false);
  settings4.set_cache_size_bytes(kFiveMb);
  SUPPRESS_END()

  Settings settings5;
  settings5.set_host("foo");
  settings5.set_ssl_enabled(true);
  SUPPRESS_DEPRECATED_DECLARATIONS_BEGIN()
  settings5.set_persistence_enabled(true);
  settings5.set_cache_size_bytes(kSixMb);
  SUPPRESS_END()

  // This is the same as settings4.
  Settings settings6;
  settings6.set_host("foo");
  settings6.set_ssl_enabled(true);
  SUPPRESS_DEPRECATED_DECLARATIONS_BEGIN()
  settings6.set_persistence_enabled(false);
  settings6.set_cache_size_bytes(kFiveMb);
  SUPPRESS_END()

  EXPECT_TRUE(settings1 == settings1);
  EXPECT_TRUE(settings6 == settings4);

  EXPECT_FALSE(settings1 == settings2);
  EXPECT_FALSE(settings1 == settings3);
  EXPECT_FALSE(settings1 == settings4);
  EXPECT_FALSE(settings1 == settings5);
  EXPECT_FALSE(settings2 == settings3);
  EXPECT_FALSE(settings2 == settings4);
  EXPECT_FALSE(settings2 == settings5);
  EXPECT_FALSE(settings3 == settings4);
  EXPECT_FALSE(settings3 == settings5);
  EXPECT_FALSE(settings4 == settings5);

  EXPECT_FALSE(settings1 != settings1);
  EXPECT_FALSE(settings6 != settings4);

  EXPECT_TRUE(settings1 != settings2);
  EXPECT_TRUE(settings1 != settings3);
  EXPECT_TRUE(settings1 != settings4);
  EXPECT_TRUE(settings1 != settings5);
  EXPECT_TRUE(settings2 != settings3);
  EXPECT_TRUE(settings2 != settings4);
  EXPECT_TRUE(settings2 != settings5);
  EXPECT_TRUE(settings3 != settings4);
  EXPECT_TRUE(settings3 != settings5);
  EXPECT_TRUE(settings4 != settings5);
}

TEST(SettingsTest, EqualityWithLocalCacheSettings) {
  constexpr int64_t kFiveMb = 5 * 1024 * 1024;
  constexpr int64_t kSixMb = 6 * 1024 * 1024;

  Settings settings1;
  settings1.set_host("foo");
  settings1.set_ssl_enabled(true);
  settings1.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::PersistentCacheSettings().WithSizeBytes(kFiveMb)));

  Settings settings2;
  settings2.set_host("bar");
  settings2.set_ssl_enabled(true);
  settings2.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::PersistentCacheSettings().WithSizeBytes(kFiveMb)));

  Settings settings3;
  settings3.set_host("foo");
  settings3.set_ssl_enabled(false);
  settings3.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::PersistentCacheSettings().WithSizeBytes(kFiveMb)));

  Settings settings4;
  settings4.set_host("foo");
  settings4.set_ssl_enabled(true);
  settings4.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::MemoryCacheSettings()));

  Settings settings5;
  settings5.set_host("foo");
  settings5.set_ssl_enabled(true);
  settings5.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::PersistentCacheSettings().WithSizeBytes(kSixMb)));

  Settings settings6;
  settings6.set_host("foo");
  settings6.set_ssl_enabled(true);
  settings6.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::MemoryCacheSettings().WithGarbageCollectorSettings(
          LocalCacheSettings::MemoryCacheSettings::EagerGCSettings())));

  Settings settings7;
  settings7.set_host("foo");
  settings7.set_ssl_enabled(true);
  settings7.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::MemoryCacheSettings().WithGarbageCollectorSettings(
          LocalCacheSettings::MemoryCacheSettings::LruGCSettings()
              .WithSizeBytes(kFiveMb))));

  Settings settings8;
  settings8.set_host("foo");
  settings8.set_ssl_enabled(true);
  settings8.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::MemoryCacheSettings().WithGarbageCollectorSettings(
          LocalCacheSettings::MemoryCacheSettings::LruGCSettings()
              .WithSizeBytes(kSixMb))));

  // Same as settings7
  Settings settings9;
  settings9.set_host("foo");
  settings9.set_ssl_enabled(true);
  settings9.set_local_cache_settings(LocalCacheSettings().WithCacheSettings(
      LocalCacheSettings::MemoryCacheSettings().WithGarbageCollectorSettings(
          LocalCacheSettings::MemoryCacheSettings::LruGCSettings()
              .WithSizeBytes(kFiveMb))));

  EXPECT_TRUE(settings1 == settings1);
  EXPECT_TRUE(settings6 == settings4);
  EXPECT_TRUE(settings7 == settings9);

  EXPECT_FALSE(settings1 == settings2);
  EXPECT_FALSE(settings1 == settings3);
  EXPECT_FALSE(settings1 == settings4);
  EXPECT_FALSE(settings1 == settings5);
  EXPECT_FALSE(settings2 == settings3);
  EXPECT_FALSE(settings2 == settings4);
  EXPECT_FALSE(settings2 == settings5);
  EXPECT_FALSE(settings3 == settings4);
  EXPECT_FALSE(settings3 == settings5);
  EXPECT_FALSE(settings4 == settings5);
  EXPECT_FALSE(settings6 == settings7);
  EXPECT_FALSE(settings7 == settings8);

  EXPECT_FALSE(settings1 != settings1);
  EXPECT_FALSE(settings6 != settings4);
  EXPECT_FALSE(settings7 != settings9);

  EXPECT_TRUE(settings1 != settings2);
  EXPECT_TRUE(settings1 != settings3);
  EXPECT_TRUE(settings1 != settings4);
  EXPECT_TRUE(settings1 != settings5);
  EXPECT_TRUE(settings2 != settings3);
  EXPECT_TRUE(settings2 != settings4);
  EXPECT_TRUE(settings2 != settings5);
  EXPECT_TRUE(settings3 != settings4);
  EXPECT_TRUE(settings3 != settings5);
  EXPECT_TRUE(settings4 != settings5);
  EXPECT_TRUE(settings6 != settings7);
  EXPECT_TRUE(settings7 != settings8);
}

TEST(SettingsTest, EqualityAssumptionsAboutSharedPtrAreCorrect) {
  const std::shared_ptr<std::string> shared_ptr_empty1;
  const std::shared_ptr<std::string> shared_ptr_empty2;
  const auto shared_ptr1 = std::make_shared<std::string>("Test String");
  const auto shared_ptr1a = shared_ptr1;
  const auto shared_ptr1b = shared_ptr1a;
  const auto shared_ptr2 = std::make_shared<std::string>("Test String");

  EXPECT_TRUE(shared_ptr_empty1 == shared_ptr_empty1);
  EXPECT_TRUE(shared_ptr_empty1 == shared_ptr_empty2);
  EXPECT_TRUE(shared_ptr1 == shared_ptr1);
  EXPECT_TRUE(shared_ptr1 == shared_ptr1a);
  EXPECT_TRUE(shared_ptr1 == shared_ptr1b);

  EXPECT_FALSE(shared_ptr_empty1 == shared_ptr1);
  EXPECT_FALSE(shared_ptr1 == shared_ptr_empty1);
  EXPECT_FALSE(shared_ptr1 == shared_ptr2);
}

TEST(SettingsTest, EqualityAssumptionsAboutOptionalAreCorrect) {
  absl::optional<std::string> invalid_optional1;
  absl::optional<std::string> invalid_optional2;
  absl::optional<std::string> optional1("Test String");
  absl::optional<std::string> optional2("Test String");
  absl::optional<std::string> optional3("A different Test String");

  EXPECT_TRUE(invalid_optional1 == invalid_optional1);
  EXPECT_TRUE(invalid_optional1 == invalid_optional2);
  EXPECT_TRUE(optional1 == optional1);
  EXPECT_TRUE(optional1 == optional2);

  EXPECT_FALSE(invalid_optional1 == optional1);
  EXPECT_FALSE(optional1 == optional3);
}

TEST(SettingsTest, EqualityAssumptionsAboutVariantAreCorrect) {
  absl::variant<std::string, std::vector<int>> invalid_variant1;
  absl::variant<std::string, std::vector<int>> invalid_variant2;
  absl::variant<std::string, std::vector<int>> variant_with_string1("zzyzx");
  absl::variant<std::string, std::vector<int>> variant_with_string2("zzyzx");
  absl::variant<std::string, std::vector<int>> variant_with_string3("abcde");
  absl::variant<std::string, std::vector<int>> variant_with_vector1(
      std::vector<int>{1, 2, 3});
  absl::variant<std::string, std::vector<int>> variant_with_vector2(
      std::vector<int>{1, 2, 3});
  absl::variant<std::string, std::vector<int>> variant_with_vector3(
      std::vector<int>{9, 8, 7});

  EXPECT_TRUE(invalid_variant1 == invalid_variant1);
  EXPECT_TRUE(invalid_variant1 == invalid_variant2);
  EXPECT_TRUE(variant_with_string1 == variant_with_string1);
  EXPECT_TRUE(variant_with_string1 == variant_with_string2);
  EXPECT_TRUE(variant_with_vector1 == variant_with_vector1);
  EXPECT_TRUE(variant_with_vector1 == variant_with_vector2);

  EXPECT_FALSE(invalid_variant1 == variant_with_string1);
  EXPECT_FALSE(variant_with_string1 == invalid_variant1);
  EXPECT_FALSE(invalid_variant1 == variant_with_vector1);
  EXPECT_FALSE(variant_with_vector1 == invalid_variant1);
  EXPECT_FALSE(variant_with_string1 == variant_with_string3);
  EXPECT_FALSE(variant_with_string1 == variant_with_vector1);
  EXPECT_FALSE(variant_with_vector1 == variant_with_vector3);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
