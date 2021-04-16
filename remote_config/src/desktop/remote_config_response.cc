/*
 * Copyright 2020 Google LLC
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

#include "remote_config/src/desktop/remote_config_response.h"

namespace firebase {
namespace remote_config {
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

Variant RemoteConfigResponse::GetEntries() { return entries_; }

// Mark the response completed for both header and body.
void RemoteConfigResponse::MarkCompleted() {
  ResponseJson::MarkCompleted();
  if (GetBody()[0] == '\0') {
    // If the body of response flatbuffer is empty, early out.
    return;
  }
  const flatbuffers::FlatBufferBuilder& builder = parser_->builder_;
  const fbs::Response* body_fbs =
      flatbuffers::GetRoot<fbs::Response>(builder.GetBufferPointer());

  if (body_fbs && body_fbs->entries() && body_fbs->entries()->size() > 0) {
    entries_ = FlexbufferToVariant(body_fbs->entries_flexbuffer_root());
  }
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
