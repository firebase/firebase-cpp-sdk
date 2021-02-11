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

#include "app/src/include/firebase/variant.h"
#include "app/src/variant_util.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/tracked_query_manager.h"
#include "database/src/desktop/persistence/persisted_compound_write_generated.h"
#include "database/src/desktop/persistence/persisted_tracked_query_generated.h"
#include "database/src/desktop/persistence/persisted_user_write_record_generated.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/util.h"

using firebase::database::internal::persistence::CreatePersistedCompoundWrite;
using firebase::database::internal::persistence::CreatePersistedQueryParams;
using firebase::database::internal::persistence::CreatePersistedQuerySpec;
using firebase::database::internal::persistence::CreatePersistedTrackedQuery;
using firebase::database::internal::persistence::CreateTreeKeyValuePair;
using firebase::util::VariantToFlexbuffer;
using flatbuffers::FlatBufferBuilder;
using flatbuffers::Offset;
using flatbuffers::Vector;

namespace firebase {
namespace database {
namespace internal {

Variant FlexbufferVectorToVariant(const flexbuffers::Vector& vector) {
  Variant result = Variant::EmptyVector();
  result.vector().reserve(vector.size());
  for (size_t i = 0; i < vector.size(); i++) {
    result.vector().push_back(FlexbufferToVariant(vector[i]));
  }
  return result;
}

Variant FlexbufferMapToVariant(const flexbuffers::Map& map) {
  Variant result = Variant::EmptyMap();
  flexbuffers::TypedVector keys = map.Keys();
  for (size_t i = 0; i < keys.size(); i++) {
    flexbuffers::Reference key = keys[i];
    flexbuffers::Reference value = map[key.AsKey()];
    result.map()[FlexbufferToVariant(key)] = FlexbufferToVariant(value);
  }
  return result;
}

Variant FlexbufferToVariant(const flexbuffers::Reference& ref) {
  switch (ref.GetType()) {
    case flexbuffers::FBT_NULL:
      return Variant::Null();
    case flexbuffers::FBT_BOOL:
      return Variant(ref.AsBool());
    case flexbuffers::FBT_INT:
    case flexbuffers::FBT_INDIRECT_INT:
    case flexbuffers::FBT_UINT:
    case flexbuffers::FBT_INDIRECT_UINT:
      return Variant(ref.AsInt64());
    case flexbuffers::FBT_FLOAT:
    case flexbuffers::FBT_INDIRECT_FLOAT:
      return Variant(ref.AsDouble());
    case flexbuffers::FBT_STRING:
      return Variant::MutableStringFromStaticString(ref.AsString().c_str());
    case flexbuffers::FBT_KEY:
      return Variant::MutableStringFromStaticString(ref.AsKey());
    case flexbuffers::FBT_MAP:
      return FlexbufferMapToVariant(ref.AsMap());
    case flexbuffers::FBT_VECTOR_BOOL:
    case flexbuffers::FBT_VECTOR_FLOAT2:
    case flexbuffers::FBT_VECTOR_FLOAT3:
    case flexbuffers::FBT_VECTOR_FLOAT4:
    case flexbuffers::FBT_VECTOR_FLOAT:
    case flexbuffers::FBT_VECTOR_INT2:
    case flexbuffers::FBT_VECTOR_INT3:
    case flexbuffers::FBT_VECTOR_INT4:
    case flexbuffers::FBT_VECTOR_INT:
    case flexbuffers::FBT_VECTOR_KEY:
    case flexbuffers::FBT_VECTOR_STRING_DEPRECATED:
    case flexbuffers::FBT_VECTOR_UINT2:
    case flexbuffers::FBT_VECTOR_UINT3:
    case flexbuffers::FBT_VECTOR_UINT4:
    case flexbuffers::FBT_VECTOR_UINT:
    case flexbuffers::FBT_VECTOR:
      return FlexbufferVectorToVariant(ref.AsVector());

    case flexbuffers::FBT_BLOB:
      LogError("Flexbuffers containing blobs are not supported.");
      break;
  }
  return Variant::Null();
}

static void VariantTreeFromFlatbuffer(const persistence::VariantTreeNode* node,
                                      Tree<Variant>* out_tree) {
  if (node->value()) {
    out_tree->set_value(FlexbufferToVariant(node->value_flexbuffer_root()));
  }
  if (node->children()) {
    for (const persistence::TreeKeyValuePair* kvp : *node->children()) {
      const char* key = kvp->key()->c_str();
      const persistence::VariantTreeNode* value = kvp->subtree();
      Tree<Variant>* subtree = out_tree->GetOrMakeSubtree(Path(key));
      VariantTreeFromFlatbuffer(value, subtree);
    }
  }
}

CompoundWrite CompoundWriteFromFlatbuffer(
    const persistence::PersistedCompoundWrite* persisted_compound_write) {
  if (!persisted_compound_write->write_tree()) {
    return CompoundWrite();
  }
  Tree<Variant> write_tree;
  const persistence::VariantTreeNode* node =
      persisted_compound_write->write_tree();
  VariantTreeFromFlatbuffer(node, &write_tree);
  return CompoundWrite(write_tree);
}

QueryParams QueryParamsFromFlatbuffer(
    const persistence::PersistedQueryParams* persisted_query_params) {
  QueryParams params;
  // Set by Query::OrderByPriority(), Query::OrderByChild(),
  // Query::OrderByKey(), and Query::OrderByValue().
  // Default is kOrderByPriority.
  params.order_by =
      static_cast<QueryParams::OrderBy>(persisted_query_params->order_by());
  if (persisted_query_params->order_by_child()) {
    params.order_by_child = persisted_query_params->order_by_child()->str();
  }
  if (persisted_query_params->start_at_value()) {
    params.start_at_value = FlexbufferToVariant(
        persisted_query_params->start_at_value_flexbuffer_root());
  }
  if (persisted_query_params->start_at_child_key()) {
    params.start_at_child_key =
        persisted_query_params->start_at_child_key()->str();
  }
  if (persisted_query_params->end_at_value()) {
    params.end_at_value = FlexbufferToVariant(
        persisted_query_params->end_at_value_flexbuffer_root());
  }
  if (persisted_query_params->end_at_child_key()) {
    params.end_at_child_key = persisted_query_params->end_at_child_key()->str();
  }
  if (persisted_query_params->equal_to_value()) {
    params.equal_to_value = FlexbufferToVariant(
        persisted_query_params->equal_to_value_flexbuffer_root());
  }
  if (persisted_query_params->equal_to_child_key()) {
    params.equal_to_child_key =
        persisted_query_params->equal_to_child_key()->str();
  }
  params.limit_first = persisted_query_params->limit_first();
  params.limit_last = persisted_query_params->limit_last();
  return params;
}

QuerySpec QuerySpecFromFlatbuffer(
    const persistence::PersistedQuerySpec* persisted_query_spec) {
  QuerySpec query_spec;
  if (persisted_query_spec->path()) {
    query_spec.path = Path(persisted_query_spec->path()->c_str());
  }
  if (persisted_query_spec->params()) {
    query_spec.params =
        QueryParamsFromFlatbuffer(persisted_query_spec->params());
  }
  return query_spec;
}

TrackedQuery TrackedQueryFromFlatbuffer(
    const persistence::PersistedTrackedQuery* persisted_tracked_query) {
  TrackedQuery tracked_query;
  tracked_query.query_id = persisted_tracked_query->query_id();
  if (persisted_tracked_query->query_spec()) {
    tracked_query.query_spec =
        QuerySpecFromFlatbuffer(persisted_tracked_query->query_spec());
  }
  tracked_query.last_use = persisted_tracked_query->last_use();
  tracked_query.complete = persisted_tracked_query->complete();
  tracked_query.active = persisted_tracked_query->active();
  return tracked_query;
}

UserWriteRecord UserWriteRecordFromFlatbuffer(
    const persistence::PersistedUserWriteRecord* persisted_user_write_record) {
  UserWriteRecord user_write_record;
  user_write_record.write_id = persisted_user_write_record->write_id();
  if (persisted_user_write_record->path()) {
    user_write_record.path = Path(persisted_user_write_record->path()->c_str());
  }
  user_write_record.is_overwrite = persisted_user_write_record->is_overwrite();
  if (user_write_record.is_overwrite) {
    if (persisted_user_write_record->overwrite()) {
      user_write_record.overwrite = FlexbufferToVariant(
          persisted_user_write_record->overwrite_flexbuffer_root());
    }
  } else {
    if (persisted_user_write_record->merge()) {
      user_write_record.merge =
          CompoundWriteFromFlatbuffer(persisted_user_write_record->merge());
    }
  }
  user_write_record.visible = persisted_user_write_record->visible();
  return user_write_record;
}

static Offset<persistence::VariantTreeNode> FlatbufferFromVariantTreeNode(
    FlatBufferBuilder* builder, const Tree<Variant>& node) {
  Offset<Vector<uint8_t>> value_offset = 0;
  if (node.value().has_value()) {
    value_offset =
        builder->CreateVector(VariantToFlexbuffer(node.value().value()));
  }
  Offset<Vector<Offset<persistence::TreeKeyValuePair>>> children_vector_offset =
      0;
  if (!node.children().empty()) {
    std::vector<Offset<persistence::TreeKeyValuePair>> children_offsets;
    for (auto& kvp : node.children()) {
      const std::string& key = kvp.first;
      const Tree<Variant>& value = kvp.second;
      children_offsets.push_back(CreateTreeKeyValuePair(
          *builder, builder->CreateString(key),
          FlatbufferFromVariantTreeNode(builder, value)));
    }
    children_vector_offset =
        builder->CreateVector<Offset<persistence::TreeKeyValuePair>>(
            children_offsets);
  }
  return CreateVariantTreeNode(*builder, value_offset, children_vector_offset);
}

Offset<persistence::PersistedCompoundWrite> FlatbufferFromCompoundWrite(
    FlatBufferBuilder* builder, const CompoundWrite& compound_write) {
  return persistence::CreatePersistedCompoundWrite(
      *builder,
      FlatbufferFromVariantTreeNode(builder, compound_write.write_tree()));
}

Offset<persistence::PersistedQueryParams> FlatbufferFromQueryParams(
    FlatBufferBuilder* builder, const QueryParams& params) {
  return CreatePersistedQueryParams(
      *builder, static_cast<persistence::OrderBy>(params.order_by),
      builder->CreateString(params.order_by_child),
      builder->CreateVector(VariantToFlexbuffer(params.start_at_value)),
      builder->CreateString(params.start_at_child_key),
      builder->CreateVector(VariantToFlexbuffer(params.end_at_value)),
      builder->CreateString(params.end_at_child_key),
      builder->CreateVector(VariantToFlexbuffer(params.equal_to_value)),
      builder->CreateString(params.equal_to_child_key), params.limit_first,
      params.limit_last);
}

Offset<persistence::PersistedQuerySpec> FlatbufferFromQuerySpec(
    FlatBufferBuilder* builder, const QuerySpec& query_spec) {
  return CreatePersistedQuerySpec(
      *builder, builder->CreateString(query_spec.path.str()),
      FlatbufferFromQueryParams(builder, query_spec.params));
}

flatbuffers::Offset<persistence::PersistedTrackedQuery>
FlatbufferFromTrackedQuery(flatbuffers::FlatBufferBuilder* builder,
                           const TrackedQuery& tracked_query) {
  return CreatePersistedTrackedQuery(
      *builder, tracked_query.query_id,
      FlatbufferFromQuerySpec(builder, tracked_query.query_spec),
      tracked_query.last_use, tracked_query.complete, tracked_query.active);
}

flatbuffers::Offset<persistence::PersistedUserWriteRecord>
FlatbufferFromUserWriteRecord(flatbuffers::FlatBufferBuilder* builder,
                              const UserWriteRecord& user_write_record) {
  return CreatePersistedUserWriteRecord(
      *builder, user_write_record.write_id,
      builder->CreateString(user_write_record.path.str()),
      user_write_record.is_overwrite
          ? builder->CreateVector(
                VariantToFlexbuffer(user_write_record.overwrite))
          : 0,
      user_write_record.is_overwrite
          ? 0
          : FlatbufferFromCompoundWrite(builder, user_write_record.merge),
      user_write_record.visible, user_write_record.is_overwrite);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
