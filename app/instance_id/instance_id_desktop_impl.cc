// Copyright 2019 Google LLC
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

#include "app/instance_id/instance_id_desktop_impl.h"

#include <assert.h>

#include "app/instance_id/iid_data_generated.h"
#include "app/src/app_identifier.h"
#include "app/src/cleanup_notifier.h"

namespace firebase {
namespace instance_id {
namespace internal {

using firebase::app::secure::UserSecureManager;

std::map<App*, InstanceIdDesktopImpl*>
    InstanceIdDesktopImpl::instance_id_by_app_;          // NOLINT
Mutex InstanceIdDesktopImpl::instance_id_by_app_mutex_;  // NOLINT

InstanceIdDesktopImpl::InstanceIdDesktopImpl(App* app)
    : storage_semaphore_(0), app_(app) {
  future_manager().AllocFutureApi(this, kInstanceIdFnCount);

  std::string package_name = app->options().package_name();
  std::string project_id = app->options().project_id();
  std::string app_identifier;

  user_secure_manager_ = MakeUnique<UserSecureManager>(
      "iid", firebase::internal::CreateAppIdentifierFromOptions(app_->options())
                 .c_str());

  // Guarded through GetInstance() already.
  instance_id_by_app_[app] = this;

  // Cleanup this object if app is destroyed.
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  assert(notifier);
  notifier->RegisterObject(this, [](void* object) {
    // Since this is going to be shared by many other modules, ex. function and
    // instance_id, nothing should delete InstanceIdDesktopImpl until App is
    // deleted.
    // TODO(chkuang): Maybe reference count by module?
    InstanceIdDesktopImpl* instance_id_to_delete =
        reinterpret_cast<InstanceIdDesktopImpl*>(object);
    LogDebug(
        "InstanceIdDesktopImpl object 0x%08x is deleted when the App 0x%08x it "
        "depends upon is deleted.",
        static_cast<int>(reinterpret_cast<intptr_t>(instance_id_to_delete)),
        static_cast<int>(
            reinterpret_cast<intptr_t>(&instance_id_to_delete->app())));
    delete instance_id_to_delete;
  });
}

InstanceIdDesktopImpl::~InstanceIdDesktopImpl() {
  {
    MutexLock lock(instance_id_by_app_mutex_);
    auto it = instance_id_by_app_.find(app_);
    if (it == instance_id_by_app_.end()) return;
    assert(it->second == this);
    instance_id_by_app_.erase(it);
  }

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_);
  assert(notifier);
  notifier->UnregisterObject(this);

  // Make sure all the pending REST requests are either cancelled or resolved
  // here.
}

InstanceIdDesktopImpl* InstanceIdDesktopImpl::GetInstance(App* app) {
  MutexLock lock(instance_id_by_app_mutex_);
  auto it = instance_id_by_app_.find(app);
  return it != instance_id_by_app_.end() ? it->second
                                         : new InstanceIdDesktopImpl(app);
}

Future<std::string> InstanceIdDesktopImpl::GetId() {
  // TODO(b/132622932)
  SafeFutureHandle<std::string> handle =
      ref_future()->SafeAlloc<std::string>(kInstanceIdFnGetId);
  ref_future()->Complete(handle, kErrorUnavailable, "Not Implemented yet");

  return MakeFuture(ref_future(), handle);
}

Future<std::string> InstanceIdDesktopImpl::GetIdLastResult() {
  return static_cast<const Future<std::string>&>(
      ref_future()->LastResult(kInstanceIdFnGetId));
}

Future<void> InstanceIdDesktopImpl::DeleteId() {
  // TODO(b/132621850)
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kInstanceIdFnRemoveId);
  ref_future()->Complete(handle, kErrorUnavailable, "Not Implemented yet");

  return MakeFuture(ref_future(), handle);
}

Future<void> InstanceIdDesktopImpl::DeleteIdLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kInstanceIdFnRemoveId));
}

Future<std::string> InstanceIdDesktopImpl::GetToken() {
  // TODO(b/132622932)
  SafeFutureHandle<std::string> handle =
      ref_future()->SafeAlloc<std::string>(kInstanceIdFnGetToken);
  ref_future()->Complete(handle, kErrorUnavailable, "Not Implemented yet");

  return MakeFuture(ref_future(), handle);
}

Future<std::string> InstanceIdDesktopImpl::GetTokenLastResult() {
  return static_cast<const Future<std::string>&>(
      ref_future()->LastResult(kInstanceIdFnGetToken));
}

Future<void> InstanceIdDesktopImpl::DeleteToken() {
  // TODO(b/132621850)
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kInstanceIdFnRemoveToken);
  ref_future()->Complete(handle, kErrorUnavailable, "Not Implemented yet");

  return MakeFuture(ref_future(), handle);
}

Future<void> InstanceIdDesktopImpl::DeleteTokenLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kInstanceIdFnRemoveToken));
}

// Save the instance ID to local secure storage.
bool InstanceIdDesktopImpl::SaveToStorage() {
  // Build up a serialized buffer algorithmically:
  flatbuffers::FlatBufferBuilder builder;

  auto iid_data_table = CreateInstanceIdDesktopDataDirect(
      builder, instance_id_.c_str(), checkin_data_.device_id.c_str(),
      checkin_data_.security_token.c_str(), expiration_time_);
  builder.Finish(iid_data_table);

  std::string save_string;
  auto bufferpointer =
      reinterpret_cast<const char*>(builder.GetBufferPointer());
  save_string.assign(bufferpointer, bufferpointer + builder.GetSize());
  // Encode flatbuffer
  std::string encoded;
  UserSecureManager::BinaryToAscii(save_string, &encoded);

  Future<void> future =
      user_secure_manager_->SaveUserData(app_->name(), encoded);

  future.OnCompletion(
      [](const Future<void>& result, void* sem_void) {
        Semaphore* sem_ptr = reinterpret_cast<Semaphore*>(sem_void);
        sem_ptr->Post();
      },
      &storage_semaphore_);
  storage_semaphore_.Wait();
  return future.error() == 0;
}

// Load the instance ID from local secure storage.
bool InstanceIdDesktopImpl::LoadFromStorage() {
  Future<std::string> future = user_secure_manager_->LoadUserData(app_->name());

  future.OnCompletion(
      [](const Future<std::string>& result, void* sem_void) {
        Semaphore* sem_ptr = reinterpret_cast<Semaphore*>(sem_void);
        sem_ptr->Post();
      },
      &storage_semaphore_);
  storage_semaphore_.Wait();
  if (future.error() == 0) {
    ReadStoredInstanceIdData(*future.result());
  }
  return future.error() == 0;
}

// Delete the instance ID from local secure storage.
bool InstanceIdDesktopImpl::DeleteFromStorage() {
  Future<void> future = user_secure_manager_->DeleteUserData(app_->name());

  future.OnCompletion(
      [](const Future<void>& result, void* sem_void) {
        Semaphore* sem_ptr = reinterpret_cast<Semaphore*>(sem_void);
        sem_ptr->Post();
      },
      &storage_semaphore_);
  storage_semaphore_.Wait();
  return future.error() == 0;
}

// Returns true if data was successfully read.
bool InstanceIdDesktopImpl::ReadStoredInstanceIdData(
    const std::string& loaded_string) {
  // Decode to flatbuffer
  std::string decoded;
  if (!UserSecureManager::AsciiToBinary(loaded_string, &decoded)) {
    LogWarning("Error decoding saved Instance ID.");
    return false;
  }
  // Verify the Flatbuffer is valid.
  flatbuffers::Verifier verifier(
      reinterpret_cast<const uint8_t*>(decoded.c_str()), decoded.length());
  if (!VerifyInstanceIdDesktopDataBuffer(verifier)) {
    LogWarning("Error verifying saved Instance ID.");
    return false;
  }

  auto iid_data_fb = GetInstanceIdDesktopData(decoded.c_str());
  if (iid_data_fb == nullptr) {
    LogWarning("Error reading table for saved Instance ID.");
    return false;
  }

  instance_id_ = iid_data_fb->instance_id()->c_str();
  checkin_data_.device_id = iid_data_fb->device_id()->c_str();
  checkin_data_.security_token = iid_data_fb->security_token()->c_str();
  expiration_time_ = iid_data_fb->expiration_time();
  return true;
}

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase
