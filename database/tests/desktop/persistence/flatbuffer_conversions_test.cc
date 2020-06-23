// Copyright 2020 Google LLC
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

#include "database/src/desktop/persistence/flatbuffer_conversions.h"

#include <iostream>
#include <vector>

#include "app/src/include/firebase/variant.h"
#include "app/src/variant_util.h"
#include "app/tests/flexbuffer_matcher.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/persistence/persisted_compound_write_generated.h"
#include "database/src/desktop/persistence/persisted_query_params_generated.h"
#include "database/src/desktop/persistence/persisted_query_spec_generated.h"
#include "database/src/desktop/persistence/persisted_tracked_query_generated.h"
#include "database/src/desktop/persistence/persisted_user_write_record_generated.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"

using firebase::database::internal::persistence::CreatePersistedCompoundWrite;
using firebase::database::internal::persistence::CreatePersistedQueryParams;
using firebase::database::internal::persistence::CreatePersistedQuerySpec;
using firebase::database::internal::persistence::CreatePersistedTrackedQuery;
using firebase::database::internal::persistence::CreateTreeKeyValuePair;
using firebase::database::internal::persistence::CreateVariantTreeNode;

using firebase::database::internal::persistence::
    FinishPersistedCompoundWriteBuffer;
using firebase::database::internal::persistence::
    FinishPersistedQueryParamsBuffer;
using firebase::database::internal::persistence::FinishPersistedQuerySpecBuffer;
using firebase::database::internal::persistence::
    FinishPersistedTrackedQueryBuffer;
using firebase::database::internal::persistence::
    FinishPersistedUserWriteRecordBuffer;

using firebase::util::VariantToFlexbuffer;
using flatbuffers::FlatBufferBuilder;
using flatbuffers::Offset;

// This makes it easier to understand what all the 0's mean.
static const int kFlatbufferEmptyField = 0;

namespace firebase {
namespace database {
namespace internal {
namespace {

// This is just to make some of the tests clearer.
typedef std::vector<Offset<persistence::TreeKeyValuePair>>
    VectorOfKeyValuePairs;

TEST(FlatBufferConversion, CompoundWriteFromFlatbuffer) {
  FlatBufferBuilder builder;

  // clang-format off
  FinishPersistedCompoundWriteBuffer(
      builder,
      CreatePersistedCompoundWrite(builder, CreateVariantTreeNode(
          builder,
          kFlatbufferEmptyField,
          builder.CreateVector(VectorOfKeyValuePairs{
              CreateTreeKeyValuePair(
                  builder,
                  builder.CreateString("aaa"),
                  CreateVariantTreeNode(
                      builder,
                      kFlatbufferEmptyField,
                      builder.CreateVector(VectorOfKeyValuePairs{
                          CreateTreeKeyValuePair(
                              builder,
                              builder.CreateString("bbb"),
                              CreateVariantTreeNode(
                                  builder,
                                  builder.CreateVector(
                                      VariantToFlexbuffer(100)),
                                  kFlatbufferEmptyField))
                      })
                  )
              )
          })
      ))
  );
  // clang-format on

  const persistence::PersistedCompoundWrite* persisted_compound_write =
      persistence::GetPersistedCompoundWrite(builder.GetBufferPointer());
  CompoundWrite result = CompoundWriteFromFlatbuffer(persisted_compound_write);

  CompoundWrite expected_result;
  expected_result.AddWriteInline(Path("aaa/bbb"), 100);

  EXPECT_EQ(result, expected_result);
}

TEST(FlatBufferConversion, QueryParamsFromFlatbuffer) {
  FlatBufferBuilder builder;

  // clang-format off
  FinishPersistedQueryParamsBuffer(
      builder,
      persistence::CreatePersistedQueryParams(
          builder,
          persistence::OrderBy_Value,
          builder.CreateString("order_by_child"),
          builder.CreateVector(VariantToFlexbuffer(1234)),
          builder.CreateString("start_at"),
          builder.CreateVector(VariantToFlexbuffer(9876)),
          builder.CreateString("end_at"),
          builder.CreateVector(VariantToFlexbuffer(5555)),
          builder.CreateString("equal_to"),
          3333,
          6666));
  // clang-format on

  const persistence::PersistedQueryParams* persisted_query_params =
      persistence::GetPersistedQueryParams(builder.GetBufferPointer());
  QueryParams result = QueryParamsFromFlatbuffer(persisted_query_params);

  QueryParams expected_result;
  expected_result.order_by = QueryParams::kOrderByValue;
  expected_result.order_by_child = "order_by_child";
  expected_result.start_at_value = Variant::FromInt64(1234);
  expected_result.start_at_child_key = "start_at";
  expected_result.end_at_value = Variant::FromInt64(9876);
  expected_result.end_at_child_key = "end_at";
  expected_result.equal_to_value = Variant::FromInt64(5555);
  expected_result.equal_to_child_key = "equal_to";
  expected_result.limit_first = 3333;
  expected_result.limit_last = 6666;

  EXPECT_EQ(result, expected_result);
}

TEST(FlatBufferConversion, QuerySpecFromFlatbuffer) {
  FlatBufferBuilder builder;

  // clang-format off
  FinishPersistedQuerySpecBuffer(
      builder,
      CreatePersistedQuerySpec(
          builder,
          builder.CreateString("this/is/a/path/to/a/thing"),
          CreatePersistedQueryParams(
              builder,
              persistence::OrderBy_Value,
              builder.CreateString("order_by_child"),
              builder.CreateVector(VariantToFlexbuffer(1234)),
              builder.CreateString("start_at"),
              builder.CreateVector(VariantToFlexbuffer(9876)),
              builder.CreateString("end_at"),
              builder.CreateVector(VariantToFlexbuffer(5555)),
              builder.CreateString("equal_to"),
              3333,
              6666)));
  // clang-format on

  const persistence::PersistedQuerySpec* persisted_query_spec =
      persistence::GetPersistedQuerySpec(builder.GetBufferPointer());
  QuerySpec result = QuerySpecFromFlatbuffer(persisted_query_spec);

  QuerySpec expected_result;
  expected_result.params.order_by = QueryParams::kOrderByValue;
  expected_result.params.order_by_child = "order_by_child";
  expected_result.params.start_at_value = Variant::FromInt64(1234);
  expected_result.params.start_at_child_key = "start_at";
  expected_result.params.end_at_value = Variant::FromInt64(9876);
  expected_result.params.end_at_child_key = "end_at";
  expected_result.params.equal_to_value = Variant::FromInt64(5555);
  expected_result.params.equal_to_child_key = "equal_to";
  expected_result.params.limit_first = 3333;
  expected_result.params.limit_last = 6666;
  expected_result.path = Path("this/is/a/path/to/a/thing");

  EXPECT_EQ(result, expected_result);
}

TEST(FlatBufferConversion, TrackedQueryFromFlatbuffer) {
  FlatBufferBuilder builder;

  // clang-format off
  FinishPersistedTrackedQueryBuffer(
      builder,
      CreatePersistedTrackedQuery(
          builder,
          9999,
          CreatePersistedQuerySpec(
              builder,
              builder.CreateString("some/path"),
              CreatePersistedQueryParams(
                  builder,
                  persistence::OrderBy_Value,
                  builder.CreateString("order_by_child"),
                  kFlatbufferEmptyField,
                  kFlatbufferEmptyField,
                  kFlatbufferEmptyField,
                  kFlatbufferEmptyField,
                  kFlatbufferEmptyField,
                  kFlatbufferEmptyField,
                  0,
                  0)),
          543024000,
          false,
          true));
  // clang-format on

  const persistence::PersistedTrackedQuery* persisted_query_spec =
      persistence::GetPersistedTrackedQuery(builder.GetBufferPointer());
  TrackedQuery result = TrackedQueryFromFlatbuffer(persisted_query_spec);

  TrackedQuery expected_result;
  expected_result.query_id = 9999;
  expected_result.query_spec.params.order_by = QueryParams::kOrderByValue;
  expected_result.query_spec.params.order_by_child = "order_by_child";
  expected_result.query_spec.path = Path("some/path");
  expected_result.last_use = 543024000;
  expected_result.complete = false;
  expected_result.active = true;

  EXPECT_EQ(result, expected_result);
}

TEST(FlatBufferConversion, UserWriteRecordFromFlatbuffer_Overwrite) {
  FlatBufferBuilder builder;

  // clang-format off
  FinishPersistedUserWriteRecordBuffer(
      builder,
      persistence::CreatePersistedUserWriteRecord(
          builder,
          1234,
          builder.CreateString("this/is/a/path/to/a/thing"),
          builder.CreateVector(VariantToFlexbuffer("flexbuffer")),
          kFlatbufferEmptyField,
          true,
          true));
  // clang-format on

  const persistence::PersistedUserWriteRecord* persisted_user_write_record =
      persistence::GetPersistedUserWriteRecord(builder.GetBufferPointer());
  UserWriteRecord result =
      UserWriteRecordFromFlatbuffer(persisted_user_write_record);

  UserWriteRecord expected_result(1234, Path("this/is/a/path/to/a/thing"),
                                  Variant::FromStaticString("flexbuffer"),
                                  true);

  EXPECT_EQ(result, expected_result);
}

TEST(FlatBufferConversion, UserWriteRecordFromFlatbuffer_Merge) {
  FlatBufferBuilder builder;

  // clang-format off
  FinishPersistedUserWriteRecordBuffer(
      builder,
      persistence::CreatePersistedUserWriteRecord(
          builder,
          1234,
          builder.CreateString("this/is/a/path/to/a/thing"),
          kFlatbufferEmptyField,
          CreatePersistedCompoundWrite(
              builder,
              CreateVariantTreeNode(
                  builder,
                  kFlatbufferEmptyField,
                  builder.CreateVector(
                      VectorOfKeyValuePairs{
                          CreateTreeKeyValuePair(
                              builder,
                              builder.CreateString("aaa"),
                              CreateVariantTreeNode(
                              builder,
                              builder.CreateVector(VariantToFlexbuffer(100)),
                              kFlatbufferEmptyField))
                      }))),
          true,
          false));
  // clang-format on

  const persistence::PersistedUserWriteRecord* persisted_user_write_record =
      persistence::GetPersistedUserWriteRecord(builder.GetBufferPointer());
  UserWriteRecord result =
      UserWriteRecordFromFlatbuffer(persisted_user_write_record);

  UserWriteRecord expected_result(
      1234, Path("this/is/a/path/to/a/thing"),
      CompoundWrite::FromPathMerge(
          std::map<Path, Variant>{{Path("aaa"), Variant(100)}}));

  EXPECT_EQ(result, expected_result);
}

TEST(FlatBufferConversion, FlatbufferFromPersistedCompoundWrite) {
  FlatBufferBuilder builder;

  FinishPersistedCompoundWriteBuffer(
      builder,
      FlatbufferFromCompoundWrite(
          &builder, CompoundWrite::FromPathMerge(std::map<Path, Variant>{
                        {Path("aaa/bbb"), Variant(100)}})));

  const persistence::PersistedCompoundWrite* result =
      persistence::GetPersistedCompoundWrite(builder.GetBufferPointer());

  EXPECT_EQ(result->write_tree()->value(), nullptr);

  EXPECT_EQ(result->write_tree()->children()->size(), 1);
  const persistence::TreeKeyValuePair* node_aaa =
      result->write_tree()->children()->Get(0);
  EXPECT_STREQ(node_aaa->key()->c_str(), "aaa");
  EXPECT_EQ(node_aaa->subtree()->value(), nullptr);

  EXPECT_EQ(node_aaa->subtree()->children()->size(), 1);
  const persistence::TreeKeyValuePair* node_bbb =
      node_aaa->subtree()->children()->Get(0);
  EXPECT_STREQ(node_bbb->key()->c_str(), "bbb");
  EXPECT_THAT(node_bbb->subtree()->value_flexbuffer_root(),
              EqualsFlexbuffer(VariantToFlexbuffer(100)));
}

TEST(FlatBufferConversion, FlatbufferFromQueryParams) {
  QueryParams query_params;
  query_params.order_by = QueryParams::kOrderByValue;
  query_params.order_by_child = "order_by_child";
  query_params.start_at_value = Variant::FromInt64(1234);
  query_params.start_at_child_key = "start_at";
  query_params.end_at_value = Variant::FromInt64(9876);
  query_params.end_at_child_key = "end_at";
  query_params.equal_to_value = Variant::FromInt64(5555);
  query_params.equal_to_child_key = "equal_to";
  query_params.limit_first = 3333;
  query_params.limit_last = 6666;
  FlatBufferBuilder builder;

  FinishPersistedQueryParamsBuffer(
      builder, FlatbufferFromQueryParams(&builder, query_params));

  const persistence::PersistedQueryParams* result =
      persistence::GetPersistedQueryParams(builder.GetBufferPointer());

  EXPECT_EQ(result->order_by(), persistence::OrderBy_Value);
  EXPECT_STREQ(result->order_by_child()->c_str(), "order_by_child");
  EXPECT_THAT(result->start_at_value_flexbuffer_root(),
              EqualsFlexbuffer(VariantToFlexbuffer(1234)));
  EXPECT_STREQ(result->start_at_child_key()->c_str(), "start_at");
  EXPECT_THAT(result->end_at_value_flexbuffer_root(),
              EqualsFlexbuffer(VariantToFlexbuffer(9876)));
  EXPECT_STREQ(result->end_at_child_key()->c_str(), "end_at");
  EXPECT_THAT(result->equal_to_value_flexbuffer_root(),
              EqualsFlexbuffer(VariantToFlexbuffer(5555)));
  EXPECT_STREQ(result->equal_to_child_key()->c_str(), "equal_to");
  EXPECT_EQ(result->limit_first(), 3333);
  EXPECT_EQ(result->limit_last(), 6666);
}

TEST(FlatBufferConversion, FlatbufferFromQuerySpec) {
  Path path("this/is/a/test/path");
  QueryParams query_params;
  query_params.order_by = QueryParams::kOrderByValue;
  query_params.order_by_child = "order_by_child";
  query_params.start_at_value = Variant::FromInt64(1234);
  query_params.start_at_child_key = "start_at";
  query_params.end_at_value = Variant::FromInt64(9876);
  query_params.end_at_child_key = "end_at";
  query_params.equal_to_value = Variant::FromInt64(5555);
  query_params.equal_to_child_key = "equal_to";
  query_params.limit_first = 3333;
  query_params.limit_last = 6666;
  FlatBufferBuilder builder;
  QuerySpec query_spec(path, query_params);

  FinishPersistedQuerySpecBuffer(builder,
                                 FlatbufferFromQuerySpec(&builder, query_spec));

  const persistence::PersistedQuerySpec* result =
      persistence::GetPersistedQuerySpec(builder.GetBufferPointer());

  EXPECT_STREQ(result->path()->c_str(), "this/is/a/test/path");
  EXPECT_EQ(result->params()->order_by(), persistence::OrderBy_Value);
  EXPECT_STREQ(result->params()->order_by_child()->c_str(), "order_by_child");
  EXPECT_THAT(result->params()->start_at_value_flexbuffer_root(),
              EqualsFlexbuffer(VariantToFlexbuffer(1234)));
  EXPECT_STREQ(result->params()->start_at_child_key()->c_str(), "start_at");
  EXPECT_THAT(result->params()->end_at_value_flexbuffer_root(),
              EqualsFlexbuffer(VariantToFlexbuffer(9876)));
  EXPECT_STREQ(result->params()->end_at_child_key()->c_str(), "end_at");
  EXPECT_THAT(result->params()->equal_to_value_flexbuffer_root(),
              EqualsFlexbuffer(VariantToFlexbuffer(5555)));
  EXPECT_STREQ(result->params()->equal_to_child_key()->c_str(), "equal_to");
  EXPECT_EQ(result->params()->limit_first(), 3333);
  EXPECT_EQ(result->params()->limit_last(), 6666);
}

TEST(FlatBufferConversion, FlatbufferFromTrackedQuery) {
  FlatBufferBuilder builder;
  TrackedQuery tracked_query;
  tracked_query.query_id = 100;
  tracked_query.query_spec.path = Path("aaa/bbb/ccc");
  tracked_query.query_spec.params.order_by = QueryParams::kOrderByValue;
  tracked_query.last_use = 1234;
  tracked_query.complete = true;
  tracked_query.active = true;

  FinishPersistedTrackedQueryBuffer(
      builder, FlatbufferFromTrackedQuery(&builder, tracked_query));

  const persistence::PersistedTrackedQuery* result =
      persistence::GetPersistedTrackedQuery(builder.GetBufferPointer());

  EXPECT_EQ(result->query_id(), 100);
  EXPECT_STREQ(result->query_spec()->path()->c_str(), "aaa/bbb/ccc");
  EXPECT_EQ(result->query_spec()->params()->order_by(),
            persistence::OrderBy_Value);
  EXPECT_EQ(result->last_use(), 1234);
  EXPECT_TRUE(result->complete());
  EXPECT_TRUE(result->active());
}

TEST(FlatBufferConversion, FlatbufferFromUserWriteRecord) {
  FlatBufferBuilder builder;
  UserWriteRecord user_write_record;
  user_write_record.write_id = 123;
  user_write_record.path = Path("aaa/bbb/ccc");
  user_write_record.overwrite = Variant("this is a string");
  user_write_record.visible = true;
  user_write_record.is_overwrite = true;

  FinishPersistedUserWriteRecordBuffer(
      builder, FlatbufferFromUserWriteRecord(&builder, user_write_record));
  const persistence::PersistedUserWriteRecord* result =
      persistence::GetPersistedUserWriteRecord(builder.GetBufferPointer());

  EXPECT_EQ(result->write_id(), 123);
  EXPECT_STREQ(result->path()->c_str(), "aaa/bbb/ccc");
  EXPECT_THAT(result->overwrite_flexbuffer_root(),
              EqualsFlexbuffer(VariantToFlexbuffer("this is a string")));
  EXPECT_EQ(result->visible(), true);
  EXPECT_EQ(result->is_overwrite(), true);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
