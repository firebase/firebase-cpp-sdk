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

#include "instance_id/src/desktop/instance_id_internal.h"
#include "instance_id/src/instance_id_internal.h"

namespace firebase {
namespace instance_id {

using internal::InstanceIdInternal;

int64_t InstanceId::creation_time() const { return 0; }

Future<std::string> InstanceId::GetId() const {
  if (!instance_id_internal_) return Future<std::string>();

  const auto future_handle = instance_id_internal_->FutureAlloc<std::string>(
      InstanceIdInternal::kApiFunctionGetId);

  if (instance_id_internal_->impl()) {
    const auto internal_future = instance_id_internal_->impl()->GetId();
    InstanceIdInternal::InternalRef& internal_ref =
        instance_id_internal_->safe_ref();
    internal_future.OnCompletion(
        [&internal_ref, future_handle](const Future<std::string>& result) {
          InstanceIdInternal::InternalRefLock lock(&internal_ref);
          if (lock.GetReference() == nullptr) {
            return;  // deleted.
          }
          if (result.error() == 0) {
            lock.GetReference()->future_api().CompleteWithResult(
                future_handle, kErrorNone, "", std::string(*result.result()));
          } else {
            lock.GetReference()->future_api().Complete(
                future_handle, kErrorUnknown, result.error_message());
          }
        });
  } else {
    // If there is no InstanceIdDesktopImpl, run as a stub.
    instance_id_internal_->future_api().CompleteWithResult(
        future_handle, kErrorNone, "", std::string("FakeId"));
  }
  return MakeFuture(&instance_id_internal_->future_api(), future_handle);
}

Future<void> InstanceId::DeleteId() {
  if (!instance_id_internal_) return Future<void>();

  const auto future_handle = instance_id_internal_->FutureAlloc<void>(
      InstanceIdInternal::kApiFunctionDeleteId);

  if (instance_id_internal_->impl()) {
    const auto internal_future = instance_id_internal_->impl()->DeleteId();

    InstanceIdInternal::InternalRef& internal_ref =
        instance_id_internal_->safe_ref();
    internal_future.OnCompletion(
        [&internal_ref, future_handle](const Future<void>& result) {
          InstanceIdInternal::InternalRefLock lock(&internal_ref);
          if (lock.GetReference() == nullptr) {
            return;  // deleted.
          }
          if (result.error() == 0) {
            lock.GetReference()->future_api().Complete(future_handle,
                                                       kErrorNone, "");
          } else {
            lock.GetReference()->future_api().Complete(
                future_handle, kErrorUnknown, result.error_message());
          }
        });
  } else {
    // If there is no InstanceIdDesktopImpl, run as a stub.
    instance_id_internal_->future_api().Complete(future_handle, kErrorNone, "");
  }
  return MakeFuture(&instance_id_internal_->future_api(), future_handle);
}

Future<std::string> InstanceId::GetToken(const char* entity,
                                         const char* scope) {
  if (!instance_id_internal_) return Future<std::string>();

  const auto future_handle = instance_id_internal_->FutureAlloc<std::string>(
      InstanceIdInternal::kApiFunctionGetToken);

  if (instance_id_internal_->impl()) {
    const auto internal_future = instance_id_internal_->impl()->GetToken(scope);

    InstanceIdInternal::InternalRef& internal_ref =
        instance_id_internal_->safe_ref();
    internal_future.OnCompletion(
        [&internal_ref, future_handle](const Future<std::string>& result) {
          InstanceIdInternal::InternalRefLock lock(&internal_ref);
          if (lock.GetReference() == nullptr) {
            return;  // deleted.
          }
          if (result.error() == 0) {
            lock.GetReference()->future_api().CompleteWithResult(
                future_handle, kErrorNone, "", std::string(*result.result()));
          } else {
            lock.GetReference()->future_api().Complete(
                future_handle, kErrorUnknown, result.error_message());
          }
        });
  } else {
    // If there is no InstanceIdDesktopImpl, run as a stub.
    instance_id_internal_->future_api().CompleteWithResult(
        future_handle, kErrorNone, "", std::string("FakeToken"));
  }
  return MakeFuture(&instance_id_internal_->future_api(), future_handle);
}

Future<void> InstanceId::DeleteToken(const char* entity, const char* scope) {
  if (!instance_id_internal_) return Future<void>();

  const auto future_handle = instance_id_internal_->FutureAlloc<void>(
      InstanceIdInternal::kApiFunctionDeleteToken);

  if (instance_id_internal_->impl()) {
    const auto internal_future =
        instance_id_internal_->impl()->DeleteToken(scope);
    InstanceIdInternal::InternalRef& internal_ref =
        instance_id_internal_->safe_ref();

    internal_future.OnCompletion(
        [&internal_ref, future_handle](const Future<void>& result) {
          InstanceIdInternal::InternalRefLock lock(&internal_ref);
          if (lock.GetReference() == nullptr) {
            return;  // deleted.
          }
          if (result.error() == 0) {
            lock.GetReference()->future_api().Complete(future_handle,
                                                       kErrorNone, "");
          } else {
            lock.GetReference()->future_api().Complete(
                future_handle, kErrorUnknown, result.error_message());
          }
        });
  } else {
    // If there is no InstanceIdDesktopImpl, run as a stub.
    instance_id_internal_->future_api().Complete(future_handle, kErrorNone, "");
  }
  return MakeFuture(&instance_id_internal_->future_api(), future_handle);
}

InstanceId* InstanceId::GetInstanceId(App* app, InitResult* init_result_out) {
  MutexLock lock(InstanceIdInternal::mutex());
  if (init_result_out) *init_result_out = kInitResultSuccess;
  auto instance_id = InstanceIdInternal::FindInstanceIdByApp(app);
  if (instance_id) return instance_id;
  return new InstanceId(app, new InstanceIdInternal(app));
}

}  // namespace instance_id
}  // namespace firebase
