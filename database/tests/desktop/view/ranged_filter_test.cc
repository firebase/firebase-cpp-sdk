// Copyright 2019 Google LLC
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

#include "database/src/desktop/view/ranged_filter.h"

#include "app/src/variant_util.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(RangedFilter, Constructor) {
  // Test the assert condition in the RangedFilter. The filter must have one of
  // the parameters set that affects the range of the query.
  {
    QueryParams params;
    params.start_at_child_key = "the_beginning";
    RangedFilter filter(params);
  }
  {
    QueryParams params;
    params.start_at_value = Variant("the_beginning_value");
    RangedFilter filter(params);
  }
  {
    QueryParams params;
    params.end_at_child_key = "the_end";
    RangedFilter filter(params);
  }
  {
    QueryParams params;
    params.end_at_value = Variant("fin");
    RangedFilter filter(params);
  }
  {
    QueryParams params;
    params.equal_to_child_key = "specific_key";
    RangedFilter filter(params);
  }
  {
    QueryParams params;
    params.equal_to_value = Variant("specific_value");
    RangedFilter filter(params);
  }
}

TEST(RangedFilter, UpdateChildWithChildKeyFilter) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByKey;
  params.start_at_value = "ccc";
  RangedFilter filter(params);

  Variant data = std::map<Variant, Variant>{
      std::make_pair("ccc", 300),
      std::make_pair("ddd", 400),
      std::make_pair("eee", 500),
  };
  IndexedVariant old_snapshot(data, params);

  // Add a new value that is outside of the range, which should not change
  // the result.
  IndexedVariant result =
      filter.UpdateChild(old_snapshot, "aaa", 100, Path(), nullptr, nullptr);

  IndexedVariant expected_result(data, params);
  EXPECT_EQ(result, expected_result);

  // Now add a new value that is inside the allowed range, and the result
  // should update.
  IndexedVariant new_result =
      filter.UpdateChild(old_snapshot, "fff", 600, Path(), nullptr, nullptr);

  Variant new_expected_data = std::map<Variant, Variant>{
      std::make_pair("ccc", 300),
      std::make_pair("ddd", 400),
      std::make_pair("eee", 500),
      std::make_pair("fff", 600),
  };
  IndexedVariant new_expected_result(new_expected_data, params);

  EXPECT_EQ(new_result, new_expected_result);
}

TEST(RangedFilter, UpdateFullVariant) {
  // Leaf
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;
    params.start_at_value = "bbb";
    params.end_at_value = "ddd";
    RangedFilter filter(params);

    IndexedVariant old_snapshot(Variant::EmptyMap(), params);
    IndexedVariant new_snapshot(1000, params);
    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    EXPECT_EQ(result, IndexedVariant(Variant::Null(), params));
  }

  // Map
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;
    params.start_at_value = "bbb";
    params.end_at_value = "ddd";
    RangedFilter filter(params);

    Variant data = std::map<Variant, Variant>{
        std::make_pair("aaa", 100), std::make_pair("bbb", 200),
        std::make_pair("ccc", 300), std::make_pair("ddd", 400),
        std::make_pair("eee", 500),
    };
    IndexedVariant old_snapshot(Variant::EmptyMap(), params);
    IndexedVariant new_snapshot(data, params);
    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);

    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("bbb", 200),
        std::make_pair("ccc", 300),
        std::make_pair("ddd", 400),
    };
    IndexedVariant expected_result(expected_data, params);

    EXPECT_EQ(result, expected_result);
  }
}

TEST(RangedFilter, UpdatePriority) {
  QueryParams params;
  params.start_at_child_key = "aaa";
  RangedFilter filter(params);

  Variant data = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  Variant priority = 9999;
  IndexedVariant old_snapshot(data, params);

  // Same as old_snapshot.
  IndexedVariant expected_value(data, params);
  EXPECT_EQ(filter.UpdatePriority(old_snapshot, priority), expected_value);
}

TEST(RangedFilter, FiltersVariants) {
  QueryParams params;
  params.start_at_child_key = "aaa";
  RangedFilter filter(params);
  EXPECT_TRUE(filter.FiltersVariants());
}

TEST(RangedFilter, StartAndEndPost) {
  // Priority
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByPriority;
    params.start_at_child_key = "aaa";
    params.start_at_value = "bbb";
    params.end_at_child_key = "ccc";
    params.end_at_value = "ddd";
    RangedFilter filter(params);

    std::pair<Variant, Variant> start_post = filter.start_post();
    std::pair<Variant, Variant> end_post = filter.end_post();
    std::pair<Variant, Variant> expected_start_post = std::make_pair(
        "aaa", std::map<Variant, Variant>{std::make_pair(".priority", "bbb")});
    std::pair<Variant, Variant> expected_end_post = std::make_pair(
        "ccc", std::map<Variant, Variant>{std::make_pair(".priority", "ddd")});

    EXPECT_EQ(start_post, expected_start_post);
    EXPECT_EQ(end_post, expected_end_post);
  }

  // Child
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByChild;
    params.start_at_child_key = "aaa";
    params.start_at_value = "bbb";
    params.end_at_child_key = "ccc";
    params.end_at_value = "ddd";
    params.order_by_child = "zzz";
    RangedFilter filter(params);

    std::pair<Variant, Variant> start_post = filter.start_post();
    std::pair<Variant, Variant> end_post = filter.end_post();
    std::pair<Variant, Variant> expected_start_post = std::make_pair(
        "aaa", std::map<Variant, Variant>{std::make_pair("zzz", "bbb")});
    std::pair<Variant, Variant> expected_end_post = std::make_pair(
        "ccc", std::map<Variant, Variant>{std::make_pair("zzz", "ddd")});

    EXPECT_EQ(start_post, expected_start_post)
        << util::VariantToJson(start_post.first) << " | "
        << util::VariantToJson(start_post.second);
    EXPECT_EQ(end_post, expected_end_post)
        << util::VariantToJson(end_post.first) << " | "
        << util::VariantToJson(end_post.second);
  }

  // Key
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;
    params.start_at_child_key = "aaa";
    params.start_at_value = "bbb";
    params.end_at_child_key = "ccc";
    params.end_at_value = "ddd";
    RangedFilter filter(params);

    std::pair<Variant, Variant> start_post = filter.start_post();
    std::pair<Variant, Variant> end_post = filter.end_post();
    std::pair<Variant, Variant> expected_start_post =
        std::make_pair("bbb", Variant::Null());
    std::pair<Variant, Variant> expected_end_post =
        std::make_pair("ddd", Variant::Null());

    EXPECT_EQ(start_post, expected_start_post);
    EXPECT_EQ(end_post, expected_end_post);
  }

  // Value
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByValue;
    params.start_at_child_key = "aaa";
    params.start_at_value = "bbb";
    params.end_at_child_key = "ccc";
    params.end_at_value = "ddd";
    RangedFilter filter(params);

    std::pair<Variant, Variant> start_post = filter.start_post();
    std::pair<Variant, Variant> end_post = filter.end_post();
    std::pair<Variant, Variant> expected_start_post =
        std::make_pair("aaa", "bbb");
    std::pair<Variant, Variant> expected_end_post =
        std::make_pair("ccc", "ddd");

    EXPECT_EQ(start_post, expected_start_post);
    EXPECT_EQ(end_post, expected_end_post);
  }
}

TEST(RangedFilter, MatchesByPriority) {
  // StartAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByPriority;
    params.start_at_child_key = "ccc";
    params.start_at_value = 300;
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_FALSE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 200)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "aaa", std::map<Variant, Variant>{std::make_pair(".priority", 100)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "bbb", std::map<Variant, Variant>{std::make_pair(".priority", 200)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "bbb", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ddd", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ddd", std::map<Variant, Variant>{std::make_pair(".priority", 400)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "eee", std::map<Variant, Variant>{std::make_pair(".priority", 500)})));
  }

  // EndAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByPriority;
    params.end_at_child_key = "ccc";
    params.end_at_value = 300;
    RangedFilter filter(params);

    EXPECT_TRUE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_TRUE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 400)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_TRUE(filter.Matches(std::make_pair("eee", 500)));

    EXPECT_TRUE(filter.Matches(std::make_pair(
        "aaa", std::map<Variant, Variant>{std::make_pair(".priority", 100)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "bbb", std::map<Variant, Variant>{std::make_pair(".priority", 200)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "bbb", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ddd", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ddd", std::map<Variant, Variant>{std::make_pair(".priority", 400)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "eee", std::map<Variant, Variant>{std::make_pair(".priority", 500)})));
  }

  // StartAt and EndAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByPriority;
    params.start_at_child_key = "bbb";
    params.start_at_value = 200;
    params.end_at_child_key = "ddd";
    params.end_at_value = 400;
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_FALSE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 200)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "aaa", std::map<Variant, Variant>{std::make_pair(".priority", 100)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "bbb", std::map<Variant, Variant>{std::make_pair(".priority", 200)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ddd", std::map<Variant, Variant>{std::make_pair(".priority", 400)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "eee", std::map<Variant, Variant>{std::make_pair(".priority", 500)})));
  }

  // EqualTo
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByPriority;
    params.equal_to_child_key = "ccc";
    params.equal_to_value = 300;
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_FALSE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 200)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ccc", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "aaa", std::map<Variant, Variant>{std::make_pair(".priority", 100)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "bbb", std::map<Variant, Variant>{std::make_pair(".priority", 200)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "bbb", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ddd", std::map<Variant, Variant>{std::make_pair(".priority", 300)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ddd", std::map<Variant, Variant>{std::make_pair(".priority", 400)})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "eee", std::map<Variant, Variant>{std::make_pair(".priority", 500)})));
  }
}

TEST(RangedFilter, MatchesByChild) {
  // StartAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByChild;
    params.start_at_child_key = "ccc";
    params.start_at_value = 300;
    params.order_by_child = "zzz/yyy";
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "bbb",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 300)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 200)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 300)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 400)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ddd",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 300)})})));

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair("zzz", Variant::Null())})));
  }

  // EndAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByChild;
    params.end_at_child_key = "ccc";
    params.end_at_value = 300;
    params.order_by_child = "zzz/yyy";
    RangedFilter filter(params);

    EXPECT_TRUE(filter.Matches(std::make_pair(
        "bbb",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 300)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 200)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 300)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 400)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ddd",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 300)})})));

    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair("zzz", Variant::Null())})));
  }

  // StartAt and EndAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByChild;
    params.start_at_child_key = "bbb";
    params.start_at_value = 200;
    params.end_at_child_key = "ddd";
    params.end_at_value = 400;
    params.order_by_child = "zzz/yyy";
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "aaa",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 100)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "bbb",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 100)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "bbb",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 200)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 300)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ddd",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 400)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ddd",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 500)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "eee",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 500)})})));

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair("zzz", Variant::Null())})));
  }

  // EqualTo
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByChild;
    params.equal_to_child_key = "ccc";
    params.equal_to_value = 300;
    params.order_by_child = "zzz/yyy";
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "aaa",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 100)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "bbb",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 100)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "bbb",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 200)})})));
    EXPECT_TRUE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 300)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ddd",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 400)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ddd",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 500)})})));
    EXPECT_FALSE(filter.Matches(std::make_pair(
        "eee",
        std::map<Variant, Variant>{std::make_pair(
            "zzz", std::map<Variant, Variant>{std::make_pair("yyy", 500)})})));

    EXPECT_FALSE(filter.Matches(std::make_pair(
        "ccc",
        std::map<Variant, Variant>{std::make_pair("zzz", Variant::Null())})));
  }
}

TEST(RangedFilter, MatchesByKey) {
  // StartAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;
    params.start_at_value = "ccc";
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_FALSE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_TRUE(filter.Matches(std::make_pair("eee", 500)));
  }

  // EndAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;
    params.end_at_value = "ccc";
    RangedFilter filter(params);

    EXPECT_TRUE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_TRUE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));
  }

  // StartAt and EndAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;
    params.start_at_value = "bbb";
    params.end_at_value = "ddd";
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_TRUE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));
  }

  // EqualTo
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;
    params.equal_to_value = "ccc";
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_FALSE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));
  }
}

TEST(RangedFilter, MatchesByValue) {
  // StartAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByValue;
    params.start_at_child_key = "ccc";
    params.start_at_value = 300;
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_FALSE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_TRUE(filter.Matches(std::make_pair("eee", 500)));
  }

  // EndAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByValue;
    params.end_at_child_key = "ccc";
    params.end_at_value = 300;
    RangedFilter filter(params);

    EXPECT_TRUE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_TRUE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));
  }

  // StartAt and EndAt
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByValue;
    params.start_at_child_key = "bbb";
    params.start_at_value = 200;
    params.end_at_child_key = "ddd";
    params.end_at_value = 400;
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_TRUE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));
  }

  // EqualTo
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByValue;
    params.equal_to_child_key = "ccc";
    params.equal_to_value = 300;
    RangedFilter filter(params);

    EXPECT_FALSE(filter.Matches(std::make_pair("aaa", 100)));
    EXPECT_FALSE(filter.Matches(std::make_pair("bbb", 200)));
    EXPECT_TRUE(filter.Matches(std::make_pair("ccc", 300)));
    EXPECT_FALSE(filter.Matches(std::make_pair("ddd", 400)));
    EXPECT_FALSE(filter.Matches(std::make_pair("eee", 500)));
  }
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
