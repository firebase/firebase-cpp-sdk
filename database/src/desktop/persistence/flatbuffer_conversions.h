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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_FLATBUFFER_CONVERSIONS_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_FLATBUFFER_CONVERSIONS_H_

#include "app/src/include/firebase/variant.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/tracked_query_manager.h"
#include "database/src/desktop/persistence/persisted_compound_write_generated.h"
#include "database/src/desktop/persistence/persisted_query_params_generated.h"
#include "database/src/desktop/persistence/persisted_query_spec_generated.h"
#include "database/src/desktop/persistence/persisted_tracked_query_generated.h"
#include "database/src/desktop/persistence/persisted_user_write_record_generated.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "flatbuffers/flatbuffers.h"

namespace firebase {
namespace database {
namespace internal {

// These functions convert serialized flatbuffers into their in-memory
// counterparts.
//
// TODO(amablue): Consider using the Flatbuffers object API here, and removing
// the non-generated versions of these structs.
CompoundWrite CompoundWriteFromFlatbuffer(
    const persistence::PersistedCompoundWrite* persisted_compound_write);
QueryParams QueryParamsFromFlatbuffer(
    const persistence::PersistedQueryParams* persisted_query_params);
QuerySpec QuerySpecFromFlatbuffer(
    const persistence::PersistedQuerySpec* persisted_query_spec);
TrackedQuery TrackedQueryFromFlatbuffer(
    const persistence::PersistedTrackedQuery* persisted_tracked_query);
UserWriteRecord UserWriteRecordFromFlatbuffer(
    const persistence::PersistedUserWriteRecord* persisted_user_write_record);

// These functions convert in-memory data structures into their flatbuffer
// counterparts so they can be serialzied to disk.
flatbuffers::Offset<persistence::PersistedCompoundWrite>
FlatbufferFromCompoundWrite(flatbuffers::FlatBufferBuilder* builder,
                            const CompoundWrite& compound_write);
flatbuffers::Offset<persistence::PersistedQueryParams>
FlatbufferFromQueryParams(flatbuffers::FlatBufferBuilder* builder,
                          const QueryParams& params);
flatbuffers::Offset<persistence::PersistedQuerySpec> FlatbufferFromQuerySpec(
    flatbuffers::FlatBufferBuilder* builder, const QuerySpec& query_spec);
flatbuffers::Offset<persistence::PersistedTrackedQuery>
FlatbufferFromTrackedQuery(flatbuffers::FlatBufferBuilder* builder,
                           const TrackedQuery& tracked_query);
flatbuffers::Offset<persistence::PersistedUserWriteRecord>
FlatbufferFromUserWriteRecord(flatbuffers::FlatBufferBuilder* builder,
                              const UserWriteRecord& user_write_record);

// Copied from App's variant_util, because of problems with Blastdoor.
// Convert from a Flexbuffer reference to a Variant.
Variant FlexbufferToVariant(const flexbuffers::Reference& ref);
// Convert from a Flexbuffer map to a Variant.
Variant FlexbufferMapToVariant(const flexbuffers::Map& map);
// Convert from a Flexbuffer vector to a Variant.
Variant FlexbufferVectorToVariant(const flexbuffers::Vector& vector);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_FLATBUFFER_CONVERSIONS_H_
