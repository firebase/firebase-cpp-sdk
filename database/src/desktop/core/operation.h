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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_OPERATION_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_OPERATION_H_

#include "app/src/assert.h"
#include "app/src/optional.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/tree.h"

namespace firebase {
namespace database {
namespace internal {

// Describes the origin of an operation, whether it came from the client or the
// server.
struct OperationSource {
  enum Source { kSourceUser, kSourceServer };

  explicit OperationSource(Source _source)
      : source(_source), query_params(), tagged(false) {}

  explicit OperationSource(const Optional<QueryParams>& _query_params)
      : source(kSourceServer), query_params(_query_params), tagged(false) {}

  OperationSource(Source _source, const Optional<QueryParams>& _query_params,
                  bool _tagged)
      : source(_source), query_params(_query_params), tagged(_tagged) {
    FIREBASE_DEV_ASSERT(!tagged || source == kSourceServer);
  }

  // Whether this operation originated on the client or the server.
  const Source source;

  // The parameters, if any, that are associated with this operation. This is
  // used to determine which View the operation should apply to.
  const Optional<QueryParams> query_params;

  const bool tagged;

  static OperationSource ForServerTaggedQuery(const QueryParams& params);

  static const OperationSource kUser;
  static const OperationSource kServer;
};

enum AckStatus {
  kAckConfirm,
  kAckRevert,
};

struct Operation {
  enum Type {
    kTypeOverwrite,
    kTypeMerge,
    kTypeAckUserWrite,
    kTypeListenComplete
  };

  Operation()
      : type(),
        source(OperationSource(OperationSource::kSourceUser)),
        path(),
        snapshot(),
        children(),
        affected_tree(),
        revert() {}

  Operation(Type _type, const OperationSource& _source, const Path& _path,
            const Variant& _snapshot, const CompoundWrite& _children,
            const Tree<bool>& _affected_tree, AckStatus status)
      : type(_type),
        source(_source),
        path(_path),
        snapshot(_snapshot),
        children(_children),
        affected_tree(_affected_tree),
        revert(status == kAckRevert) {}

  // Utility constructors for building each kind of operation.
  static Operation Overwrite(const OperationSource& source, const Path& path,
                             const Variant& snapshot);
  static Operation Merge(const OperationSource& source, const Path& path,
                         const CompoundWrite& children);
  static Operation AckUserWrite(const Path& path,
                                const Tree<bool>& affected_tree,
                                AckStatus status);
  static Operation ListenComplete(const OperationSource& source,
                                  const Path& path);

  // The type of operation this object represents.
  const Type type;

  // The source of this operation.
  const OperationSource source;

  // The location of this operation in the database.
  const Path path;

  // For use with kOperationTypeOverwrite. This is the data to write at the
  // given location.
  const Variant snapshot;

  // For use with kOperationTypeMerge. This is the data to merge at the given
  // location.
  const CompoundWrite children;

  // For use with kOperationTypeAckUserWrite.
  // The set of locations that are being ackknoledged.
  const Tree<bool> affected_tree;
  // Indicates that the data was not written to the server successfully and
  // should be reverted.
  const bool revert;
};

Optional<Operation> OperationForChild(const Operation& op,
                                      const std::string& child_key);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_OPERATION_H_
