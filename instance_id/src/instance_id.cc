// Copyright 2017 Google LLC
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

#include "instance_id/src/include/firebase/instance_id.h"

#include <cstdint>
#include <string>

#include "app/src/include/firebase/version.h"
#include "instance_id/src/instance_id_internal.h"

namespace firebase {
namespace instance_id {

Mutex g_instance_ids_lock;  // NOLINT

// Wildcard scope for FCM token retrieval.
static const char* const kScopeAll = "*";

DEFINE_FIREBASE_VERSION_STRING(FirebaseInstanceId);

using internal::InstanceIdInternal;

InstanceId::InstanceId(App* app, InstanceIdInternal* instance_id_internal)
    : app_(app), instance_id_internal_(instance_id_internal) {
  MutexLock lock(g_instance_ids_lock);
  InstanceIdInternal::RegisterInstanceIdForApp(app_, this);
}

InstanceId::~InstanceId() { DeleteInternal(); }

void InstanceId::DeleteInternal() {
  MutexLock lock(g_instance_ids_lock);

  if (!instance_id_internal_) return;

  InstanceIdInternal::UnregisterInstanceIdForApp(app_, this);
  delete instance_id_internal_;
  instance_id_internal_ = nullptr;
  app_ = nullptr;
}

Future<std::string> InstanceId::GetIdLastResult() const {
  return instance_id_internal_
             ? static_cast<const Future<std::string>&>(
                   instance_id_internal_->future_api().LastResult(
                       InstanceIdInternal::kApiFunctionGetId))
             : Future<std::string>();
}

Future<void> InstanceId::DeleteIdLastResult() const {
  return instance_id_internal_
             ? static_cast<const Future<void>&>(
                   instance_id_internal_->future_api().LastResult(
                       InstanceIdInternal::kApiFunctionDeleteId))
             : Future<void>();
}

Future<std::string> InstanceId::GetToken() {
  return instance_id_internal_
             ? GetToken(app_->options().messaging_sender_id(), kScopeAll)
             : Future<std::string>();
}

Future<std::string> InstanceId::GetTokenLastResult() const {
  return instance_id_internal_
             ? static_cast<const Future<std::string>&>(
                   instance_id_internal_->future_api().LastResult(
                       InstanceIdInternal::kApiFunctionGetToken))
             : Future<std::string>();
}

Future<void> InstanceId::DeleteToken() {
  return instance_id_internal_
             ? DeleteToken(app_->options().messaging_sender_id(), kScopeAll)
             : Future<void>();
}

Future<void> InstanceId::DeleteTokenLastResult() const {
  return instance_id_internal_
             ? static_cast<const Future<void>&>(
                   instance_id_internal_->future_api().LastResult(
                       InstanceIdInternal::kApiFunctionDeleteToken))
             : Future<void>();
}

}  // namespace instance_id
}  // namespace firebase
