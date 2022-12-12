//  Copyright (c) 2022 Google LLC
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

#ifndef FIREBASE_INSTALLATIONS_SRC_DESKTOP_INSTALLATIONS_DESKTOP_H_
#define FIREBASE_INSTALLATIONS_SRC_DESKTOP_INSTALLATIONS_DESKTOP_H_

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"
#include "installations/src/desktop/rest/installations_rest.h"

namespace firebase {
namespace installations {
namespace internal {

class InstallationsInternal {
 public:
  explicit InstallationsInternal(const firebase::App& app);
  ~InstallationsInternal();

  // Platform-specific method that causes a heartbeat to be logged.
  // See go/firebase-platform-logging-design for more information.
  static void LogHeartbeat(const firebase::App& app);

  Future<std::string> GetId();
  Future<std::string> GetIdLastResult();

  Future<void> Delete();
  Future<void> DeleteLastResult();

  Future<std::string> GetToken(bool forceRefresh);
  Future<std::string> GetTokenLastResult();

  bool Initialized() const;

  void Cleanup();
  std::string GetFid() { return rest_.GetFid(); }

 private:
   
  void RegisterInstallations();
  
  const firebase::App& app_;  // NOLINT
  
  mutable Mutex internal_mutex_;

  scheduler::Scheduler scheduler_;

  // Safe reference to this.  Set in constructor and cleared in destructor
  // Should be safe to be copied in any thread because the SharedPtr never
  // changes, until safe_this_ is completely destroyed.
  typedef firebase::internal::SafeReference<InstallationsInternal> ThisRef;
  typedef firebase::internal::SafeReferenceLock<InstallationsInternal>
      ThisRefLock;
  ThisRef safe_this_;
  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl_;
  InstallationsREST rest_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif /* FIREBASE_INSTALLATIONS_SRC_DESKTOP_INSTALLATIONS_DESKTOP_H_ */
