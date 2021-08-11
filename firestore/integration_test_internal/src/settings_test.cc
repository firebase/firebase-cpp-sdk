// Copyright 2021 Google LLC

#include <stdint.h>

#include "firebase/firestore.h"

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

using SettingsTest = testing::Test;

TEST_F(SettingsTest, Equality) {
  constexpr int64_t kFiveMb = 5 * 1024 * 1024;
  constexpr int64_t kSixMb = 6 * 1024 * 1024;

  Settings settings1;
  settings1.set_host("foo");
  settings1.set_ssl_enabled(true);
  settings1.set_persistence_enabled(true);
  settings1.set_cache_size_bytes(kFiveMb);

  Settings settings2;
  settings2.set_host("bar");
  settings2.set_ssl_enabled(true);
  settings2.set_persistence_enabled(true);
  settings2.set_cache_size_bytes(kFiveMb);

  Settings settings3;
  settings3.set_host("foo");
  settings3.set_ssl_enabled(false);
  settings3.set_persistence_enabled(true);
  settings3.set_cache_size_bytes(kFiveMb);

  Settings settings4;
  settings4.set_host("foo");
  settings4.set_ssl_enabled(true);
  settings4.set_persistence_enabled(false);
  settings4.set_cache_size_bytes(kFiveMb);

  Settings settings5;
  settings5.set_host("foo");
  settings5.set_ssl_enabled(true);
  settings5.set_persistence_enabled(true);
  settings5.set_cache_size_bytes(kSixMb);

  // This is the same as settings4.
  Settings settings6;
  settings6.set_host("foo");
  settings6.set_ssl_enabled(true);
  settings6.set_persistence_enabled(false);
  settings6.set_cache_size_bytes(kFiveMb);

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

}  // namespace
}  // namespace firestore
}  // namespace firebase
