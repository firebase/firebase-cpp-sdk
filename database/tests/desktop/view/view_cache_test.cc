// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "database/src/desktop/view/view_cache.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(ViewCacheTest, Constructors) {
  // Everything should be uninitialized.
  ViewCache blank_cache;
  // Local
  EXPECT_EQ(blank_cache.local_snap().variant(), Variant::Null());
  EXPECT_FALSE(blank_cache.local_snap().fully_initialized());
  EXPECT_FALSE(blank_cache.local_snap().filtered());
  // Server
  EXPECT_EQ(blank_cache.server_snap().variant(), Variant::Null());
  EXPECT_FALSE(blank_cache.server_snap().fully_initialized());
  EXPECT_FALSE(blank_cache.server_snap().filtered());

  CacheNode local_cache(IndexedVariant("local_value"), true, false);
  CacheNode server_cache(IndexedVariant("server_value"), false, true);
  ViewCache populated_cache(local_cache, server_cache);
  // Local
  EXPECT_EQ(populated_cache.local_snap().variant(), "local_value");
  EXPECT_TRUE(populated_cache.local_snap().fully_initialized());
  EXPECT_FALSE(populated_cache.local_snap().filtered());
  // Server
  EXPECT_EQ(populated_cache.server_snap().variant(), "server_value");
  EXPECT_FALSE(populated_cache.server_snap().fully_initialized());
  EXPECT_TRUE(populated_cache.server_snap().filtered());
}

TEST(ViewCacheTest, GetCompleteSnaps) {
  // Everything should be uninitialized.
  ViewCache blank_cache;
  EXPECT_EQ(blank_cache.GetCompleteLocalSnap(), nullptr);
  EXPECT_EQ(blank_cache.GetCompleteServerSnap(), nullptr);

  // Initialize the local and server cache.
  CacheNode local_cache(IndexedVariant("local_value"), true, true);
  CacheNode server_cache(IndexedVariant("server_value"), true, true);
  ViewCache populated_cache(local_cache, server_cache);
  EXPECT_EQ(populated_cache.GetCompleteLocalSnap(),
            &populated_cache.local_snap().variant());
  EXPECT_EQ(populated_cache.GetCompleteServerSnap(),
            &populated_cache.server_snap().variant());
}

TEST(ViewCacheTest, UpdateLocalSnap) {
  // Start uninitialized and update the local cache.
  ViewCache view_cache;
  ViewCache local_update =
      view_cache.UpdateLocalSnap(IndexedVariant("local_value"), true, true);
  // Local
  EXPECT_STREQ(local_update.local_snap().variant().string_value(),
               "local_value");
  EXPECT_TRUE(local_update.local_snap().fully_initialized());
  EXPECT_TRUE(local_update.local_snap().filtered());
  // Server (should be unchanged).
  EXPECT_TRUE(local_update.server_snap().variant().is_null());
  EXPECT_FALSE(local_update.server_snap().fully_initialized());
  EXPECT_FALSE(local_update.server_snap().filtered());
}

TEST(ViewCacheTest, UpdateServerSnap) {
  // Start uninitialized and update the server cache.
  ViewCache view_cache;
  ViewCache server_update =
      view_cache.UpdateServerSnap(IndexedVariant("server_value"), true, true);
  // Local (should be unchanged).
  EXPECT_TRUE(server_update.local_snap().variant().is_null());
  EXPECT_FALSE(server_update.local_snap().fully_initialized());
  EXPECT_FALSE(server_update.local_snap().filtered());
  // Server
  EXPECT_STREQ(server_update.server_snap().variant().string_value(),
               "server_value");
  EXPECT_TRUE(server_update.server_snap().fully_initialized());
  EXPECT_TRUE(server_update.server_snap().filtered());
}

TEST(ViewCacheTest, CacheNodeEquality) {
  CacheNode cache_node(IndexedVariant("some_string"), true, true);
  CacheNode same_cache_node(IndexedVariant("some_string"), true, true);
  CacheNode different_variant(IndexedVariant("different_string"), true, true);
  CacheNode different_fully_initialized(IndexedVariant("some_string"), false,
                                        true);
  CacheNode different_filtered(IndexedVariant("some_string"), true, false);

  EXPECT_EQ(cache_node, same_cache_node);
  EXPECT_NE(cache_node, different_variant);
  EXPECT_NE(cache_node, different_fully_initialized);
  EXPECT_NE(cache_node, different_filtered);
}

TEST(ViewCacheTest, ViewCacheEquality) {
  CacheNode local_cache(IndexedVariant("local_value"), true, true);
  CacheNode server_cache(IndexedVariant("server_value"), true, true);
  ViewCache view_cache(local_cache, server_cache);
  ViewCache same_view_cache(local_cache, server_cache);

  CacheNode different_local_cache_node(IndexedVariant("wrong_local_value"),
                                       true, true);
  CacheNode different_server_cache_node(IndexedVariant("server_value"), false,
                                        true);
  ViewCache different_local_cache(different_local_cache_node, server_cache);
  ViewCache different_server_cache(local_cache, different_server_cache_node);

  EXPECT_EQ(view_cache, same_view_cache);
  EXPECT_NE(view_cache, different_local_cache);
  EXPECT_NE(view_cache, different_server_cache);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
