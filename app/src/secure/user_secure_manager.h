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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_MANAGER_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_MANAGER_H_

#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/safe_reference.h"
#include "app/src/secure/user_secure_data_handle.h"
#include "app/src/secure/user_secure_internal.h"

namespace firebase {
namespace app {
namespace secure {

enum SecureOperationType {
  kLoadUserData,
  kSaveUserData,
  kDeleteUserData,
  kDeleteAllData,
};

class UserSecureManager {
 public:
  explicit UserSecureManager(const char* domain, const char* app_id);
  ~UserSecureManager();

  // Overloaded constructor to set the internal instance.
  explicit UserSecureManager(
      UniquePtr<UserSecureInternal> user_secure_internal);

  // Load persisted user data for given app name.
  Future<std::string> LoadUserData(const std::string& app_name);

  // Save user data under the key of given app name.
  Future<void> SaveUserData(const std::string& app_name,
                            const std::string& user_data);

  // Delete user data under the given app name.
  Future<void> DeleteUserData(const std::string& app_name);

  // Delete all user data.
  Future<void> DeleteAllData();

  // Decode the given ASCII string into binary data.
  static bool AsciiToBinary(const std::string& encoded, std::string* decoded);
  // Encode the given binary string into ASCII-friendly data.
  static void BinaryToAscii(const std::string& original, std::string* encoded);

 private:
  // Cancel already scheduled tasks in destruction.
  void CancelScheduledTasks();

  void CancelOperation(SecureOperationType operation_type);

  UniquePtr<UserSecureInternal> user_secure_;
  ReferenceCountedFutureImpl future_api_;

  static void CreateScheduler();
  static void DestroyScheduler();

  // Guards static scheduler pointer
  static Mutex s_scheduler_mutex_;  // NOLINT
  static scheduler::Scheduler* s_scheduler_;
  static int32_t s_scheduler_ref_count_;

  // map from operation type to scheduled request handle. Make sure only one
  // request exist in scheduler for each type.
  std::map<SecureOperationType, scheduler::RequestHandle> operation_handles_;

  // Safe reference to this.  Set in constructor and cleared in destructor
  // Should be safe to be copied in any thread because the SharedPtr never
  // changes, until safe_this_ is completely destroyed.
  typedef firebase::internal::SafeReference<UserSecureManager> ThisRef;
  typedef firebase::internal::SafeReferenceLock<UserSecureManager> ThisRefLock;
  ThisRef safe_this_;
};

}  // namespace secure
}  // namespace app
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_MANAGER_H_
