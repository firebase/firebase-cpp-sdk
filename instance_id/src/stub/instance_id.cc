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

#include <cstdint>
#include <string>

#include "instance_id/src/include/firebase/instance_id.h"
#include "instance_id/src/instance_id_internal.h"

namespace firebase {
namespace instance_id {

using internal::InstanceIdInternal;

int64_t InstanceId::creation_time() const { return 0; }

Future<std::string> InstanceId::GetId() const {
  if (!instance_id_internal_) return Future<std::string>();

  const auto future_handle = instance_id_internal_->FutureAlloc<std::string>(
      InstanceIdInternal::kApiFunctionGetId);
  instance_id_internal_->future_api().CompleteWithResult(
      future_handle, kErrorNone, "", std::string("FakeId"));
  return GetIdLastResult();
}

Future<void> InstanceId::DeleteId() {
  if (!instance_id_internal_) return Future<void>();

  const auto future_handle = instance_id_internal_->FutureAlloc<void>(
      InstanceIdInternal::kApiFunctionDeleteId);
  instance_id_internal_->future_api().Complete(future_handle, kErrorNone, "");
  return DeleteIdLastResult();
}

Future<std::string> InstanceId::GetToken(const char* entity,
                                         const char* scope) {
  if (!instance_id_internal_) return Future<std::string>();

  const auto future_handle = instance_id_internal_->FutureAlloc<std::string>(
      InstanceIdInternal::kApiFunctionGetToken);
  instance_id_internal_->future_api().CompleteWithResult(
      future_handle, kErrorNone, "", std::string("FakeToken"));
  return GetTokenLastResult();
}

Future<void> InstanceId::DeleteToken(const char* entity, const char* scope) {
  if (!instance_id_internal_) return Future<void>();

  const auto future_handle = instance_id_internal_->FutureAlloc<void>(
      InstanceIdInternal::kApiFunctionDeleteToken);
  instance_id_internal_->future_api().Complete(future_handle, kErrorNone, "");
  return DeleteTokenLastResult();
}

InstanceId* InstanceId::GetInstanceId(App* app, InitResult* init_result_out) {
  if (init_result_out) *init_result_out = kInitResultSuccess;
  auto instance_id = InstanceIdInternal::FindInstanceIdByApp(app);
  if (instance_id) return instance_id;
  return new InstanceId(app, new InstanceIdInternal());
}

}  // namespace instance_id
}  // namespace firebase
