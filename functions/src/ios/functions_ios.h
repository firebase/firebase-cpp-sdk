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
#include "app/src/util_ios.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"

#ifdef __OBJC__
@class FIRFunctions;
@class NSError;
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
  [[nodiscard]] App* app() const;

  // Return the region or custom domain we were created with.
  [[nodiscard]] const char* region() const;

  // Manage Future implementations.
  [[nodiscard]] FutureManager& future_manager() { return future_manager_; }

  [[nodiscard]] CleanupNotifier& cleanup() { return cleanup_; }

  [[nodiscard]] const std::shared_ptr<FunctionsLifetimeState>& lifetime_state()
      const {
    return lifetime_state_;
  }

  // Whether this object was successfully initialized by the constructor.
  [[nodiscard]] bool is_valid() const;
  [[nodiscard]] bool initialized() const { return is_valid(); }

  [[nodiscard]] HttpsCallableReferenceInternal* GetHttpsCallable(
      const char* name) const;
  [[nodiscard]] HttpsCallableReferenceInternal* GetHttpsCallable(
      const char* name, const HttpsCallableOptions& options) const {
    return GetHttpsCallableWithOptions(name, options);
  }
  [[nodiscard]] HttpsCallableReferenceInternal* GetHttpsCallableWithOptions(
      const char* name, const HttpsCallableOptions& options) const;
  [[nodiscard]] HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      const char* url) const;
  [[nodiscard]] HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      const char* url, const HttpsCallableOptions& options) const {
    return GetHttpsCallableFromURLWithOptions(url, options);
  }
  [[nodiscard]] HttpsCallableReferenceInternal*
  GetHttpsCallableFromURLWithOptions(const char* url,
                                     const HttpsCallableOptions& options) const;

  void UseFunctionsEmulator(const char* origin);

#ifdef __OBJC__
  [[nodiscard]] static Error ErrorFromNSError(NSError* error);

  [[nodiscard]] FIRFunctions* impl() const { return impl_->get(); }
#endif  // __OBJC__

 private:
  App* app_;
  std::string region_;
  std::unique_ptr<FIRFunctionsPointer> impl_;
  FutureManager future_manager_;
  std::shared_ptr<FunctionsLifetimeState> lifetime_state_;
  CleanupNotifier cleanup_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_IOS_FUNCTIONS_IOS_H_
