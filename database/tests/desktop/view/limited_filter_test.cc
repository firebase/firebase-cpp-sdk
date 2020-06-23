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

#include "database/src/desktop/view/limited_filter.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(LimitedFilter, Constructor) {
  {
    QueryParams params;
    params.limit_first = 2;
    LimitedFilter filter(params);
  }
  {
    QueryParams params;
    params.limit_last = 2;
    LimitedFilter filter(params);
  }
}

TEST(LimitedFilter, UpdateChildLimitFirst) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByKey;
  params.limit_first = 2;
  LimitedFilter filter(params);

  Variant data = std::map<Variant, Variant>{
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  IndexedVariant old_snapshot(data, params);

  // Prepend new value.
  {
    IndexedVariant changed_result =
        filter.UpdateChild(old_snapshot, "aaa", 100, Path(), nullptr, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("aaa", 100),
        std::make_pair("bbb", 200),
    };
    IndexedVariant expected_changed_result(expected_data, params);
    EXPECT_EQ(changed_result, expected_changed_result);
  }

  // New value at the end doesn't get appended.
  {
    IndexedVariant unchanged_result =
        filter.UpdateChild(old_snapshot, "ddd", 400, Path(), nullptr, nullptr);
    IndexedVariant expected_unchanged_result(data, params);
    EXPECT_EQ(unchanged_result, expected_unchanged_result);
  }
}

TEST(LimitedFilter, UpdateChildLimitLast) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByKey;
  params.limit_last = 2;
  LimitedFilter filter(params);

  Variant data = std::map<Variant, Variant>{
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  IndexedVariant old_snapshot(data, params);

  // New value at the beginning doesn't get prepending.
  {
    IndexedVariant unchanged_result =
        filter.UpdateChild(old_snapshot, "aaa", 100, Path(), nullptr, nullptr);
    IndexedVariant expected_unchanged_result(data, params);
    EXPECT_EQ(unchanged_result, expected_unchanged_result);
  }

  // Append new value.
  {
    IndexedVariant changed_result =
        filter.UpdateChild(old_snapshot, "ddd", 400, Path(), nullptr, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("ccc", 300),
        std::make_pair("ddd", 400),
    };
    IndexedVariant expected_changed_result(expected_data, params);
    EXPECT_EQ(changed_result, expected_changed_result);
  }
}

TEST(LimitedFilter, UpdateFullVariantLimitFirst) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByKey;
  params.limit_first = 2;
  LimitedFilter filter(params);

  Variant old_data = std::map<Variant, Variant>{
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
      std::make_pair("ddd", 400),
      std::make_pair("eee", 500),
  };
  IndexedVariant old_snapshot(old_data, params);

  // new_data removes elements at the end.
  {
    Variant new_data = std::map<Variant, Variant>{
        std::make_pair("bbb", 200),
        std::make_pair("ccc", 300),
        std::make_pair("ddd", 400),
    };
    IndexedVariant new_snapshot(new_data, params);

    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("bbb", 200),
        std::make_pair("ccc", 300),
    };
    IndexedVariant expected_result(expected_data, params);
    EXPECT_EQ(result, expected_result);
  }

  // new_data removes elements at the beginning.
  {
    Variant new_data = std::map<Variant, Variant>{
        std::make_pair("ccc", 300),
        std::make_pair("ddd", 400),
        std::make_pair("eee", 500),
    };
    IndexedVariant new_snapshot(new_data, params);

    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("ccc", 300),
        std::make_pair("ddd", 400),
    };
    IndexedVariant expected_result(expected_data, params);
    EXPECT_EQ(result, expected_result);
  }

  // new_data adds elements at the end.
  {
    Variant new_data = std::map<Variant, Variant>{
        std::make_pair("bbb", 200), std::make_pair("ccc", 300),
        std::make_pair("ddd", 400), std::make_pair("eee", 500),
        std::make_pair("fff", 600),
    };
    IndexedVariant new_snapshot(new_data, params);

    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("bbb", 200),
        std::make_pair("ccc", 300),
    };
    IndexedVariant expected_result(expected_data, params);
    EXPECT_EQ(result, expected_result);
  }

  // new_data adds elements at the beginning.
  {
    Variant new_data = std::map<Variant, Variant>{
        std::make_pair("aaa", 100), std::make_pair("bbb", 200),
        std::make_pair("ccc", 300), std::make_pair("ddd", 400),
        std::make_pair("eee", 500),
    };
    IndexedVariant new_snapshot(new_data, params);

    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("aaa", 100),
        std::make_pair("bbb", 200),
    };
    IndexedVariant expected_result(expected_data, params);
    EXPECT_EQ(result, expected_result);
  }
}

TEST(LimitedFilter, UpdateFullVariantLimitLast) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByKey;
  params.limit_last = 2;
  LimitedFilter filter(params);

  Variant old_data = std::map<Variant, Variant>{
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
      std::make_pair("ddd", 400),
      std::make_pair("eee", 500),
  };
  IndexedVariant old_snapshot(old_data, params);

  // new_data removes elements at the end.
  {
    Variant new_data = std::map<Variant, Variant>{
        std::make_pair("bbb", 200),
        std::make_pair("ccc", 300),
        std::make_pair("ddd", 400),
    };
    IndexedVariant new_snapshot(new_data, params);

    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("ccc", 300),
        std::make_pair("ddd", 400),
    };
    IndexedVariant expected_result(expected_data, params);
    EXPECT_EQ(result, expected_result);
  }

  // new_data removes elements at the beginning.
  {
    Variant new_data = std::map<Variant, Variant>{
        std::make_pair("ccc", 300),
        std::make_pair("ddd", 400),
        std::make_pair("eee", 500),
    };
    IndexedVariant new_snapshot(new_data, params);

    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("ddd", 400),
        std::make_pair("eee", 500),
    };
    IndexedVariant expected_result(expected_data, params);
    EXPECT_EQ(result, expected_result);
  }

  // new_data adds elements at the end.
  {
    Variant new_data = std::map<Variant, Variant>{
        std::make_pair("bbb", 200), std::make_pair("ccc", 300),
        std::make_pair("ddd", 400), std::make_pair("eee", 500),
        std::make_pair("fff", 600),
    };
    IndexedVariant new_snapshot(new_data, params);

    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("eee", 500),
        std::make_pair("fff", 600),
    };
    IndexedVariant expected_result(expected_data, params);
    EXPECT_EQ(result, expected_result);
  }

  // new_data adds elements at the beginning.
  {
    Variant new_data = std::map<Variant, Variant>{
        std::make_pair("aaa", 100), std::make_pair("bbb", 200),
        std::make_pair("ccc", 300), std::make_pair("ddd", 400),
        std::make_pair("eee", 500),
    };
    IndexedVariant new_snapshot(new_data, params);

    IndexedVariant result =
        filter.UpdateFullVariant(old_snapshot, new_snapshot, nullptr);
    Variant expected_data = std::map<Variant, Variant>{
        std::make_pair("ddd", 400),
        std::make_pair("eee", 500),
    };
    IndexedVariant expected_result(expected_data, params);
    EXPECT_EQ(result, expected_result);
  }
}

TEST(LimitedFilter, UpdatePriority) {
  QueryParams params;
  params.limit_last = 2;
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

TEST(LimitedFilter, FiltersVariants) {
  QueryParams params;
  params.limit_last = 2;
  LimitedFilter filter(params);
  EXPECT_TRUE(filter.FiltersVariants());
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
