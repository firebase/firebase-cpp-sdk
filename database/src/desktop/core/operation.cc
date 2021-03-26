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

#include "database/src/desktop/core/operation.h"

#include "app/src/assert.h"
#include "app/src/path.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

const OperationSource OperationSource::kUser(OperationSource::kSourceUser,
                                             Optional<QueryParams>(), false);
const OperationSource OperationSource::kServer(OperationSource::kSourceServer,
                                               Optional<QueryParams>(), false);

OperationSource OperationSource::ForServerTaggedQuery(
    const QueryParams& params) {
  return OperationSource(OperationSource::kSourceServer,
                         Optional<QueryParams>(params), true);
}

Operation Operation::Overwrite(const OperationSource& source, const Path& path,
                               const Variant& snapshot) {
  return Operation(Operation::kTypeOverwrite, source, path, snapshot,
                   CompoundWrite(), Tree<bool>(), kAckConfirm);
}

Operation Operation::Merge(const OperationSource& source, const Path& path,
                           const CompoundWrite& children) {
  return Operation(Operation::kTypeMerge, source, path, Variant(), children,
                   Tree<bool>(), kAckConfirm);
}

Operation Operation::AckUserWrite(const Path& path,
                                  const Tree<bool>& affected_tree,
                                  AckStatus status) {
  return Operation(Operation::kTypeAckUserWrite, OperationSource::kUser, path,
                   Variant(), CompoundWrite(), affected_tree, status);
}

Operation Operation::ListenComplete(const OperationSource& source,
                                    const Path& path) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      source.source != OperationSource::kSourceUser,
      "Can't have a listen complete from a user source");
  return Operation(Operation::kTypeListenComplete, source, path, Variant(),
                   CompoundWrite(), Tree<bool>(), kAckConfirm);
}

static Optional<Operation> OperationForChildOverwrite(
    const Operation& op, const std::string& child_key) {
  if (op.path.empty()) {
    return Optional<Operation>(Operation::Overwrite(
        op.source, Path(), VariantGetChild(&op.snapshot, child_key)));
  } else {
    std::vector<std::string> directories = op.path.GetDirectories();
    Path child_path(std::next(directories.begin()), directories.end());
    return Optional<Operation>(
        Operation::Overwrite(op.source, child_path, op.snapshot));
  }
}

static Optional<Operation> OperationForChildMerge(
    const Operation& op, const std::string& child_key) {
  if (op.path.empty()) {
    CompoundWrite child_tree = op.children.ChildCompoundWrite(Path(child_key));
    if (child_tree.IsEmpty()) {
      // This child is unaffected.
      return Optional<Operation>();
    } else if (child_tree.GetRootWrite().has_value()) {
      return Optional<Operation>(
          Operation::Overwrite(op.source, Path(), *child_tree.GetRootWrite()));
    } else {
      return Optional<Operation>(
          Operation::Merge(op.source, Path(), child_tree));
    }
  } else {
    std::vector<std::string> directories = op.path.GetDirectories();
    if (directories.front() == child_key) {
      auto start = directories.begin();
      auto finish = directories.end();
      return Optional<Operation>(Operation::Merge(
          op.source, Path(std::next(start), finish), op.children));
    } else {
      // Merge doesn't affect operation path.
      return Optional<Operation>();
    }
  }
}

static Optional<Operation> OperationForChildAckUserWrite(
    const Operation& op, const std::string& child_key) {
  if (!op.path.empty()) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        op.path.GetDirectories().front() == child_key,
        "OperationForChild called for unrelated child.");
    std::vector<std::string> directories = op.path.GetDirectories();
    auto start = directories.begin();
    auto finish = directories.end();
    return Optional<Operation>(Operation::AckUserWrite(
        Path(std::next(start), finish), op.affected_tree,
        op.revert ? kAckRevert : kAckConfirm));
  } else if (op.affected_tree.value().has_value()) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        op.affected_tree.children().empty(),
        "AffectedTree should not have overlapping affected paths.");
    // All child locations are affected as well; just return same operation.
    return Optional<Operation>(op);
  } else {
    const Tree<bool>* child_tree = op.affected_tree.GetChild(child_key);
    return Optional<Operation>(
        Operation::AckUserWrite(Path(), child_tree ? *child_tree : Tree<bool>(),
                                op.revert ? kAckRevert : kAckConfirm));
  }
}

static Optional<Operation> OperationForChildListenComplete(
    const Operation& op, const std::string& child_key) {
  if (op.path.empty()) {
    return Optional<Operation>(Operation::ListenComplete(op.source, Path()));
  } else {
    std::vector<std::string> directories = op.path.GetDirectories();
    auto start = directories.begin();
    auto finish = directories.end();
    return Optional<Operation>(
        Operation::ListenComplete(op.source, Path(std::next(start), finish)));
  }
}

Optional<Operation> OperationForChild(const Operation& op,
                                      const std::string& child_key) {
  switch (op.type) {
    case Operation::kTypeOverwrite: {
      return OperationForChildOverwrite(op, child_key);
    }
    case Operation::kTypeMerge: {
      return OperationForChildMerge(op, child_key);
    }
    case Operation::kTypeAckUserWrite: {
      return OperationForChildAckUserWrite(op, child_key);
    }
    case Operation::kTypeListenComplete: {
      return OperationForChildListenComplete(op, child_key);
    }
  }
  FIREBASE_DEV_ASSERT_MESSAGE(false, "Invalid Operation::Type");
  return Optional<Operation>();
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
