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
#include "app/src/time.h"
#include "flatbuffers/flexbuffers.h"

namespace firebase {
namespace instance_id {
namespace internal {

using firebase::app::secure::UserSecureManager;

// Check-in backend.
static const char kCheckinUrl[] =
    "https://device-provisioning.googleapis.com/checkin";
// Check-in refresh period (7 days) in milliseconds.
static const uint64_t kCheckinRefreshPeriodMs =
    7 * 24 * 60 * firebase::internal::kMillisecondsPerMinute;
// Request type and protocol for check-in requests.
static const int kCheckinRequestType = 2;
static const int kCheckinProtocolVersion = 2;
// Instance ID backend.
static const char kInstanceIdUrl[] = "https://fcmtoken.googleapis.com/register";

std::map<App*, InstanceIdDesktopImpl*>
    InstanceIdDesktopImpl::instance_id_by_app_;          // NOLINT
Mutex InstanceIdDesktopImpl::instance_id_by_app_mutex_;  // NOLINT

InstanceIdDesktopImpl::InstanceIdDesktopImpl(App* app)
    : storage_semaphore_(0),
      app_(app),
      locale_("en_US" /* TODO(b/132732303) */),
      timezone_("America/Los_Angeles" /* TODO(b/132733022) */),
      logging_id_(rand()),  // NOLINT
      ios_device_model_("iPhone 8" /* TODO */),
      ios_device_version_("8.0" /* TODO */),
      network_operation_complete_(0),
      terminating_(false) {
  rest::InitTransportCurl();
  transport_.reset(new rest::TransportCurl());
  (void)kInstanceIdUrl;  // TODO(smiles): Remove this when registration is in.

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
  // Cancel any pending network operations.
  {
    MutexLock lock(network_operation_mutex_);
    // All outstanding operations should complete with an error.
    terminating_ = true;
    NetworkOperation* operation = network_operation_.get();
    if (operation) operation->Cancel();
  }
  // Cancel scheduled tasks and shut down the scheduler to prevent any
  // additional tasks being executed.
  scheduler_.CancelAllAndShutdownWorkerThread();

  rest::CleanupTransportCurl();
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
  if (terminating_) return false;

  // Build up a serialized buffer algorithmically:
  flatbuffers::FlatBufferBuilder builder;

  auto iid_data_table = CreateInstanceIdDesktopDataDirect(
      builder, instance_id_.c_str(), checkin_data_.device_id.c_str(),
      checkin_data_.security_token.c_str(), checkin_data_.digest.c_str(),
      checkin_data_.last_checkin_time_ms);
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
  if (terminating_) return false;

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
  checkin_data_.digest = iid_data_fb->digest()->c_str();
  checkin_data_.last_checkin_time_ms = iid_data_fb->last_checkin_time_ms();
  return true;
}

bool InstanceIdDesktopImpl::InitialOrRefreshCheckin() {
  if (terminating_) return false;

  // Load check-in data from storage if it hasn't already been loaded.
  if (checkin_data_.last_checkin_time_ms == 0) {
    // Try loading from storage.  Since we can't tell whether this failed
    // because the data doesn't exist or if there is an I/O error, just
    // continue.
    LoadFromStorage();
  }

  // If we've already checked in.
  if (checkin_data_.last_checkin_time_ms > 0) {
    FIREBASE_ASSERT(!checkin_data_.device_id.empty() &&
                    !checkin_data_.security_token.empty() &&
                    !checkin_data_.digest.empty());
    // Make sure the device ID and token aren't expired.
    uint64_t time_elapsed_ms =
        firebase::internal::GetTimestamp() - checkin_data_.last_checkin_time_ms;
    if (time_elapsed_ms < kCheckinRefreshPeriodMs) {
      // Everything is up to date.
      return true;
    }
    checkin_data_.Clear();
  }

  // Construct the JSON request.
  flexbuffers::Builder fbb;
  struct BuilderScope {
    BuilderScope(flexbuffers::Builder* fbb_, const CheckinData* checkin_data_,
                 const char* locale_, const char* timezone_, int logging_id_,
                 const char* ios_device_model_, const char* ios_device_version_)
        : fbb(fbb_),
          checkin_data(checkin_data_),
          locale(locale_),
          timezone(timezone_),
          logging_id(logging_id_),
          ios_device_model(ios_device_model_),
          ios_device_version(ios_device_version_) {}

    flexbuffers::Builder* fbb;
    const CheckinData* checkin_data;
    const char* locale;
    const char* timezone;
    int logging_id;
    const char* ios_device_model;
    const char* ios_device_version;
  } builder_scope(&fbb, &checkin_data_, locale_.c_str(), timezone_.c_str(),
                  logging_id_, ios_device_model_.c_str(),
                  ios_device_version_.c_str());
  fbb.Map(
      [](BuilderScope& builder_scope) {
        flexbuffers::Builder* fbb = builder_scope.fbb;
        const CheckinData& checkin_data = *builder_scope.checkin_data;
        fbb->Map(
            "checkin",
            [](BuilderScope& builder_scope) {
              flexbuffers::Builder* fbb = builder_scope.fbb;
              const CheckinData& checkin_data = *builder_scope.checkin_data;
              fbb->Map(
                  "iosbuild",
                  [](BuilderScope& builder_scope) {
                    flexbuffers::Builder* fbb = builder_scope.fbb;
                    fbb->String("model", builder_scope.ios_device_model);
                    fbb->String("os_version", builder_scope.ios_device_version);
                  },
                  builder_scope);
              fbb->Int("type", kCheckinRequestType);
              fbb->Int("user_number", 0 /* unused at the moment */);
              fbb->Int("last_checkin_msec", checkin_data.last_checkin_time_ms);
            },
            builder_scope);
        fbb->Int("fragment", 0 /* unused at the moment */);
        fbb->Int("logging_id", builder_scope.logging_id);
        fbb->String("locale", builder_scope.locale);
        fbb->Int("version", kCheckinProtocolVersion);
        fbb->String("digest", checkin_data.digest.c_str());
        fbb->String("timezone", builder_scope.timezone);
        fbb->Int("user_serial_number", 0 /* unused at the moment */);
        fbb->Int("id",
                 flatbuffers::StringToInt(checkin_data.device_id.c_str()));
        fbb->Int("security_token",
                 flatbuffers::StringToInt(checkin_data.security_token.c_str()));
      },
      builder_scope);
  fbb.Finish();
  request_buffer_.clear();
  flexbuffers::GetRoot(fbb.GetBuffer()).ToString(true, true, request_buffer_);
  // Send request to the server then wait for the response or for the request
  // to be canceled by another thread.
  {
    MutexLock lock(network_operation_mutex_);
    network_operation_.reset(
        new NetworkOperation(request_buffer_, &network_operation_complete_));
    rest::Request* request = &network_operation_->request;
    request->set_url(kCheckinUrl);
    request->set_method(rest::util::kPost);
    request->add_header(rest::util::kContentType, rest::util::kApplicationJson);
    network_operation_->Perform(transport_.get());
  }
  network_operation_complete_.Wait();

  logging_id_ = rand();  // NOLINT
  {
    MutexLock lock(network_operation_mutex_);
    assert(network_operation_.get());
    const rest::Response& response = network_operation_->response;
    // Check for errors
    if (response.status() != rest::util::HttpSuccess) {
      LogError("Check-in failed with response %d '%s'", response.status(),
               response.GetBody());
      network_operation_.reset(nullptr);
      return false;
    }
    // Parse the response.
    flexbuffers::Builder fbb;
    flatbuffers::Parser parser;
    if (!parser.ParseFlexBuffer(response.GetBody(), nullptr, &fbb)) {
      LogError("Unable to parse response '%s'", response.GetBody());
      network_operation_.reset(nullptr);
      return false;
    }
    auto root = flexbuffers::GetRoot(fbb.GetBuffer()).AsMap();
    if (!root["stats_ok"].AsBool()) {
      LogError("Unexpected stats_ok field '%s' vs 'true'",
               root["stats_ok"].ToString().c_str());
      network_operation_.reset(nullptr);
      return false;
    }
    checkin_data_.device_id = root["android_id"].AsString().c_str();
    checkin_data_.security_token = root["security_token"].AsString().c_str();
    checkin_data_.digest = root["digest"].AsString().c_str();
    checkin_data_.last_checkin_time_ms = firebase::internal::GetTimestamp();
    network_operation_.reset(nullptr);
    if (!SaveToStorage()) {
      checkin_data_.Clear();
      LogError("Unable to save check-in information to storage.");
      return false;
    }
  }
  return true;
}

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase
