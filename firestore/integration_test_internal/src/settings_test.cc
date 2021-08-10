// Copyright 2021 Google LLC

#include <future>
#include <map>
#include <string>

#include "firebase/firestore.h"
#include "firestore_integration_test.h"

namespace firebase {
namespace firestore {

using SettingsTest = FirestoreIntegrationTest;

TEST_F(SettingsTest, Equality) {
  int64_t five_mb = 5 * 1024 * 1024;
  int64_t six_mb = 6 * 1024 * 1024;

  Settings settings1;
  settings1.set_host("foo");
  settings1.set_ssl_enabled(true);
  settings1.set_persistence_enabled(true);
  settings1.set_cache_size_bytes(five_mb);

  Settings settings2;
  settings2.set_host("bar");
  settings2.set_ssl_enabled(true);
  settings2.set_persistence_enabled(true);
  settings2.set_cache_size_bytes(five_mb);

  Settings settings3;
  settings3.set_host("foo");
  settings3.set_ssl_enabled(false);
  settings3.set_persistence_enabled(true);
  settings3.set_cache_size_bytes(five_mb);

  Settings settings4;
  settings4.set_host("foo");
  settings4.set_ssl_enabled(true);
  settings4.set_persistence_enabled(false);
  settings4.set_cache_size_bytes(five_mb);

  Settings settings5;
  settings5.set_host("foo");
  settings5.set_ssl_enabled(true);
  settings5.set_persistence_enabled(true);
  settings5.set_cache_size_bytes(six_mb);

  EXPECT_TRUE(settings1 == settings1);

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
}  // namespace firestore
}  // namespace firebase
