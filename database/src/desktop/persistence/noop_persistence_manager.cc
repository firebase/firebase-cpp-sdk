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

#include "database/src/desktop/persistence/noop_persistence_manager.h"

#define VERIFY_INSIDE_TRANSACTION() \
  FIREBASE_DEV_ASSERT_MESSAGE(      \
      this->inside_transaction_,    \
      "Transaction expected to already be in progress.")

namespace firebase {
namespace database {
namespace internal {

NoopPersistenceManager::NoopPersistenceManager() : inside_transaction_(false) {}

void NoopPersistenceManager::SaveUserOverwrite(const Path& path,
                                               const Variant& variant,
                                               WriteId write_id) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::SaveUserMerge(const Path& path,
                                           const CompoundWrite& children,
                                           WriteId write_id) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::RemoveUserWrite(WriteId write_id) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::RemoveAllUserWrites() {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::ApplyUserWriteToServerCache(
    const Path& path, const Variant& variant) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::ApplyUserWriteToServerCache(
    const Path& path, const CompoundWrite& merge) {
  VERIFY_INSIDE_TRANSACTION();
}

std::vector<UserWriteRecord> NoopPersistenceManager::LoadUserWrites() {
  return std::vector<UserWriteRecord>();
}

CacheNode NoopPersistenceManager::ServerCache(const QuerySpec& query_spec) {
  return CacheNode();
}

void NoopPersistenceManager::UpdateServerCache(const QuerySpec& query_spec,
                                               const Variant& variant) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::UpdateServerCache(const Path& path,
                                               const CompoundWrite& children) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::SetQueryActive(const QuerySpec& query_spec) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::SetQueryInactive(const QuerySpec& query_spec) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::SetQueryComplete(const QuerySpec& query_spec) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::SetTrackedQueryKeys(
    const QuerySpec& query_spec, const std::set<std::string>& keys) {
  VERIFY_INSIDE_TRANSACTION();
}

void NoopPersistenceManager::UpdateTrackedQueryKeys(
    const QuerySpec& query_spec, const std::set<std::string>& added,
    const std::set<std::string>& removed) {
  VERIFY_INSIDE_TRANSACTION();
}

bool NoopPersistenceManager::RunInTransaction(std::function<bool()> func) {
  FIREBASE_DEV_ASSERT_MESSAGE(!inside_transaction_,
                              "RunInTransaction called when an existing "
                              "transaction is already in progress.");
  inside_transaction_ = true;
  bool success = func();
  inside_transaction_ = false;
  return success;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
