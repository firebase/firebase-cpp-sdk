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

#include <map>
#include <string>

#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/mutex.h"

namespace firebase {
namespace instance_id {
namespace internal {

class InstanceIdDesktopImpl {
 public:
  // Future functions for Instance Id
  enum InstanceIdFn {
    kInstanceIdFnGetId = 0,
    kInstanceIdFnRemoveId,
    kInstanceIdFnGetToken,
    kInstanceIdFnRemoveToken,
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
  Future<std::string> GetToken();

  // Get the results of the most recent call to @ref GetToken.
  Future<std::string> GetTokenLastResult();

  // Revokes access to a scope (action)
  Future<void> DeleteToken();

  // Get the results of the most recent call to @ref DeleteToken.
  Future<void> DeleteTokenLastResult();

  // Gets the App this object is connected to.
  App& app() { return *app_; }

 private:
  explicit InstanceIdDesktopImpl(App* app);

  // Get future manager of this object
  FutureManager& future_manager() { return future_manager_; }

  // Get the ReferenceCountedFutureImpl for this object
  ReferenceCountedFutureImpl* ref_future() {
    return future_manager().GetFutureApi(this);
  }

  // Future manager of this object
  FutureManager future_manager_;

  // The App this object is connected to.
  App* app_;

  // Global map of App to InstanceIdDesktopImpl
  static std::map<App*, InstanceIdDesktopImpl*> instance_id_by_app_;

  // Mutex to protect instance_id_by_app_
  static Mutex instance_id_by_app_mutex_;
};

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_INSTANCE_ID_INSTANCE_ID_DESKTOP_IMPL_H_
