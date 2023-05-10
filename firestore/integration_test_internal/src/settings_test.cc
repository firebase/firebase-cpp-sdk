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

#include "firebase/firestore.h"
#include "firebase/firestore/local_cache_settings.h"
#include "firebase_test_framework.h"

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

TEST(SettingsTest, Equality) {
  constexpr int64_t kFiveMb = 5 * 1024 * 1024;
  constexpr int64_t kSixMb = 6 * 1024 * 1024;

  Settings settings1;
  settings1.set_host("foo");
  settings1.set_ssl_enabled(true);
  WITH_DEPRECATED_API(settings1.set_persistence_enabled(true));
  WITH_DEPRECATED_API(settings1.set_cache_size_bytes(kFiveMb));

  Settings settings2;
  settings2.set_host("bar");
  settings2.set_ssl_enabled(true);
  WITH_DEPRECATED_API(settings2.set_persistence_enabled(true));
  WITH_DEPRECATED_API(settings2.set_cache_size_bytes(kFiveMb));

  Settings settings3;
  settings3.set_host("foo");
  settings3.set_ssl_enabled(false);
  WITH_DEPRECATED_API(settings3.set_persistence_enabled(true));
  WITH_DEPRECATED_API(settings3.set_cache_size_bytes(kFiveMb));

  Settings settings4;
  settings4.set_host("foo");
  settings4.set_ssl_enabled(true);
  WITH_DEPRECATED_API(settings4.set_persistence_enabled(false));
  WITH_DEPRECATED_API(settings4.set_cache_size_bytes(kFiveMb));

  Settings settings5;
  settings5.set_host("foo");
  settings5.set_ssl_enabled(true);
  WITH_DEPRECATED_API(settings5.set_persistence_enabled(true));
  WITH_DEPRECATED_API(settings5.set_cache_size_bytes(kSixMb));

  // This is the same as settings4.
  Settings settings6;
  settings6.set_host("foo");
  settings6.set_ssl_enabled(true);
  WITH_DEPRECATED_API(settings6.set_persistence_enabled(false));
  WITH_DEPRECATED_API(settings6.set_cache_size_bytes(kFiveMb));

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
  settings1.set_local_cache_settings(
      PersistentCacheSettings::Create().WithSizeBytes(kFiveMb));

  Settings settings2;
  settings2.set_host("bar");
  settings2.set_ssl_enabled(true);
  settings2.set_local_cache_settings(
      PersistentCacheSettings::Create().WithSizeBytes(kFiveMb));

  Settings settings3;
  settings3.set_host("foo");
  settings3.set_ssl_enabled(false);
  settings3.set_local_cache_settings(
      PersistentCacheSettings::Create().WithSizeBytes(kFiveMb));

  Settings settings4;
  settings4.set_host("foo");
  settings4.set_ssl_enabled(true);
  settings4.set_local_cache_settings(MemoryCacheSettings::Create());

  Settings settings5;
  settings5.set_host("foo");
  settings5.set_ssl_enabled(true);
  settings5.set_local_cache_settings(
      PersistentCacheSettings::Create().WithSizeBytes(kSixMb));

  Settings settings6;
  settings6.set_host("foo");
  settings6.set_ssl_enabled(true);
  settings6.set_local_cache_settings(
      MemoryCacheSettings::Create().WithGarbageCollectorSettings(
          MemoryEagerGCSettings::Create()));

  Settings settings7;
  settings7.set_host("foo");
  settings7.set_ssl_enabled(true);
  settings7.set_local_cache_settings(
      MemoryCacheSettings::Create().WithGarbageCollectorSettings(
          MemoryLruGCSettings::Create().WithSizeBytes(kFiveMb)));

  Settings settings8;
  settings8.set_host("foo");
  settings8.set_ssl_enabled(true);
  settings8.set_local_cache_settings(
      MemoryCacheSettings::Create().WithGarbageCollectorSettings(
          MemoryLruGCSettings::Create().WithSizeBytes(kSixMb)));

  // Same as settings7
  Settings settings9;
  settings9.set_host("foo");
  settings9.set_ssl_enabled(true);
  settings9.set_local_cache_settings(
      MemoryCacheSettings::Create().WithGarbageCollectorSettings(
          MemoryLruGCSettings::Create().WithSizeBytes(kFiveMb)));

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

}  // namespace
}  // namespace firestore
}  // namespace firebase
