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

#ifndef FIREBASE_FUNCTIONS_SRC_IOS_FUNCTIONS_IOS_H_
#define FIREBASE_FUNCTIONS_SRC_IOS_FUNCTIONS_IOS_H_

#include <memory>
#include <string>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"

#ifdef __OBJC__
#import "FirebaseFunctions-Swift.h"
#endif  // __OBJC__

namespace firebase {
namespace functions {
namespace internal {

// This defines the class FIRFunctionsPointer, which is a C++-compatible wrapper
// around the FIRFunctions Obj-C class.
OBJ_C_PTR_WRAPPER(FIRFunctions);

struct FunctionsLifetimeState {
  Mutex mutex;
  bool alive = true;
};

class FunctionsInternal {
 public:
  FunctionsInternal(App* app, const char* region);
  ~FunctionsInternal();

  FunctionsInternal(const FunctionsInternal&) = delete;
  FunctionsInternal& operator=(const FunctionsInternal&) = delete;

  // Return the App we were created with.
  App* app() const { return app_; }

  // Return the region or custom domain we were created with.
  const char* region() const { return region_.c_str(); }

  // Manage Future implementations.
  FutureManager& future_manager() { return future_manager_; }

  CleanupNotifier& cleanup() { return cleanup_; }

  const std::shared_ptr<FunctionsLifetimeState>& lifetime_state() const {
    return lifetime_state_;
  }

  // Whether this object was successfully initialized by the constructor.
  bool is_valid() const;

  HttpsCallableReferenceInternal* GetHttpsCallable(const char* name) const;
  HttpsCallableReferenceInternal* GetHttpsCallableWithOptions(
      const char* name, const HttpsCallableOptions& options) const;
  HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      const char* url) const;
  HttpsCallableReferenceInternal* GetHttpsCallableFromURLWithOptions(
      const char* url, const HttpsCallableOptions& options) const;

  void UseFunctionsEmulator(const char* origin);

#ifdef __OBJC__
  static Error ErrorFromNSError(NSError* error);

  FIRFunctions* impl() const { return impl_->get(); }
#endif  // __OBJC__

 private:
  FutureManager future_manager_;
  CleanupNotifier cleanup_;
  App* app_;
  std::string region_;
  std::unique_ptr<FIRFunctionsPointer> impl_;
  std::shared_ptr<FunctionsLifetimeState> lifetime_state_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_IOS_FUNCTIONS_IOS_H_
