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

#ifndef FIREBASE_APP_CLIENT_CPP_INSTANCE_ID_INSTANCE_ID_DESKTOP_IMPL_H_
#define FIREBASE_APP_CLIENT_CPP_INSTANCE_ID_INSTANCE_ID_DESKTOP_IMPL_H_

#include <cstdint>
#include <map>
#include <string>

#include "app/memory/unique_ptr.h"
#include "app/src/callback.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/mutex.h"
#include "app/src/scheduler.h"
#include "app/src/secure/user_secure_manager.h"
#include "app/src/semaphore.h"

namespace firebase {

namespace rest {
class Request;
class Transport;
}  // namespace rest

namespace instance_id {
namespace internal {

class InstanceIdDesktopImplTest;  // For testing.
class NetworkOperation;  // Defined in instance_id_desktop_impl.cc
class SignalSemaphoreResponse;  // Defined in instance_id_desktop_impl.cc

class InstanceIdDesktopImpl {
 public:
  // Future functions for Instance Id
  enum InstanceIdFn {
    kInstanceIdFnGetId = 0,
    kInstanceIdFnRemoveId,
    kInstanceIdFnGetToken,
    kInstanceIdFnRemoveToken,
    kInstanceIdFnStorageSave,
    kInstanceIdFnStorageLoad,
    kInstanceIdFnStorageDelete,
    kInstanceIdFnCount,
  };

  // Errors for Instance Id futures
  enum Error {
    // The operation was a success, no error occurred.
    kErrorNone = 0,
    // The service is unavailable.
    kErrorUnavailable,
    // An unknown error occurred.
    kErrorUnknownError,
    // App shutdown is in progress.
    kErrorShutdown,
  };

  virtual ~InstanceIdDesktopImpl();

  // InstanceIdDesktopImpl is neither copyable nor movable.
  InstanceIdDesktopImpl(const InstanceIdDesktopImpl&) = delete;
  InstanceIdDesktopImpl& operator=(const InstanceIdDesktopImpl&) = delete;
  InstanceIdDesktopImpl(InstanceIdDesktopImpl&&) = delete;
  InstanceIdDesktopImpl& operator=(InstanceIdDesktopImpl&&) = delete;

  // Returns the InstanceIdRestImpl object for an App and creating the
  // InstanceIdRestImpl if required.
  static InstanceIdDesktopImpl* GetInstance(App* app);

  // Returns a stable identifier that uniquely identifies the app
  Future<std::string> GetId();

  // Get the results of the most recent call to @ref GetId.
  Future<std::string> GetIdLastResult();

  // Delete the ID associated with the app, revoke all tokens and
  // allocate a new ID.
  Future<void> DeleteId();

  // Get the results of the most recent call to @ref DeleteId.
  Future<void> DeleteIdLastResult();

  // Returns a token that authorizes an Entity to perform an action on
  // behalf of the application identified by Instance ID.
  Future<std::string> GetToken(const char* scope);

  // Get the results of the most recent call to @ref GetToken.
  Future<std::string> GetTokenLastResult();

  // Revokes access to a scope (action)
  Future<void> DeleteToken(const char* scope);

  // Get the results of the most recent call to @ref DeleteToken.
  Future<void> DeleteTokenLastResult();

  // Gets the App this object is connected to.
  App& app() { return *app_; }

 private:
  friend class InstanceIdDesktopImplTest;

  // Data cached from a check-in and required to perform instance ID operations.
  struct CheckinData {
    CheckinData() : last_checkin_time_ms(0) {}

    // Reset to the default state.
    void Clear() {
      security_token.clear();
      device_id.clear();
      digest.clear();
      last_checkin_time_ms = 0;
    }

    // Security token from the last check-in.
    std::string security_token;
    // Device ID from the last check-in.
    std::string device_id;
    // Digest from the previous checkin.
    std::string digest;
    // Last check-in time in milliseconds since the Linux epoch.
    uint64_t last_checkin_time_ms;
  };

  // Fetches a token with expodential backoff using the scheduler.
  class FetchServerTokenCallback : public callback::Callback {
   public:
    FetchServerTokenCallback(InstanceIdDesktopImpl* iid,
                             const std::string& scope,
                             SafeFutureHandle<std::string> future_handle)
        : iid_(iid),
          scope_(scope),
          future_handle_(future_handle),
          retry_delay_time_(0) {}

    void Run() override;

   private:
    InstanceIdDesktopImpl* iid_;
    std::string scope_;
    SafeFutureHandle<std::string> future_handle_;
    uint64_t retry_delay_time_;
  };

  explicit InstanceIdDesktopImpl(App* app);

  // Get future manager of this object
  FutureManager& future_manager() { return future_manager_; }

  // Get the ReferenceCountedFutureImpl for this object
  ReferenceCountedFutureImpl* ref_future() {
    return future_manager().GetFutureApi(this);
  }

  // Verify and parse the stored IID data, called by LoadFromStorage(), and fill
  // in this class. Returns false if there are any parsing errors.
  bool ReadStoredInstanceIdData(const std::string& loaded_string);

  // For testing only, to use a fake UserSecureManager.
  void SetUserSecureManager(
      UniquePtr<firebase::app::secure::UserSecureManager> manager) {
    user_secure_manager_ = manager;
  }

  // Save the instance ID to local secure storage. Blocking.
  bool SaveToStorage();
  // Load the instance ID from local secure storage. Blocking.
  bool LoadFromStorage();
  // Delete the instance ID from local secure storage. Blocking.
  bool DeleteFromStorage();

  // Fetch the current device ID and security token or check-in to retrieve
  // a new device ID and security token.
  bool InitialOrRefreshCheckin();

  // Generate a new appid. This is 4 bits of 0x7 followed by 60 bits of
  // randomness, then base64-encoded until golden brown.
  std::string GenerateAppId();

  // Find cached token for a scope.
  std::string FindCachedToken(const char* scope);

  // Clear a token for a scope.
  void DeleteCachedToken(const char* scope);

  // Perform a token fetch or delete network operation with an optional
  // callback to modify the request before the network operation is scheduled.
  void ServerTokenOperation(const char* scope,
                            void (*request_callback)(rest::Request* request,
                                                     void* state),
                            void* state);

  // Fetch a token from the cache or retrieve a new token from the server.
  // The scope can be either "FCM" or "*" for remote config and other users.
  // retry is set to "true" if the server response requires a retry with
  // exponential back-off.
  bool FetchServerToken(const char* scope, bool* retry);

  // Delete a server-side token for a scope and remove it from the cache.
  // If delete_id is true all tokens are deleted along with the server
  // registration of instance ID.
  bool DeleteServerToken(const char* scope, bool delete_id);

  // Used to wait for async storage functions to finish.
  Semaphore storage_semaphore_;

  // Future manager of this object
  FutureManager future_manager_;

  // The App this object is connected to.
  App* app_;

  UniquePtr<firebase::app::secure::UserSecureManager> user_secure_manager_;

  CheckinData checkin_data_;
  // Cached instance ID.
  std::string instance_id_;
  // Tokens indexed by scope.
  std::map<std::string, std::string> tokens_;

  // Locale used to check-in.
  std::string locale_;
  // Time-zone used to check-in.
  std::string timezone_;
  // Logging ID for check-in requests.
  int logging_id_;
  // iOS device to spoof.
  std::string ios_device_model_;
  // iOS device version.
  std::string ios_device_version_;
  // Application version.
  std::string app_version_;
  // Operating system version.
  std::string os_version_;
  // Platform requesting a token. 0 = UNKNOWN
  int platform_;

  // Performs network operations for this object.
  UniquePtr<rest::Transport> transport_;
  Mutex network_operation_mutex_;
  // A network operation if a request is in progress, null otherwise.
  // Guarded by network_operation_mutex_.
  UniquePtr<NetworkOperation> network_operation_;
  // Buffer for the request in progress.
  std::string request_buffer_;
  // Signaled when the current network operation is complete.
  Semaphore network_operation_complete_;
  // Serializes all network operations from this object.
  scheduler::Scheduler scheduler_;
  // Whether tasks should continue to be scheduled.
  // Guarded by network_operation_mutex_.
  bool terminating_;

  // Global map of App to InstanceIdDesktopImpl
  static std::map<App*, InstanceIdDesktopImpl*> instance_id_by_app_;

  // Mutex to protect instance_id_by_app_
  static Mutex instance_id_by_app_mutex_;
};

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_INSTANCE_ID_INSTANCE_ID_DESKTOP_IMPL_H_
