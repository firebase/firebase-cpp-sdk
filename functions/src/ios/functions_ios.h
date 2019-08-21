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

#ifndef FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_IOS_FUNCTIONS_IOS_H_
#define FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_IOS_FUNCTIONS_IOS_H_

#include <map>
#include <set>

#include "app/memory/unique_ptr.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/mutex.h"
#include "app/src/util_ios.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

#ifdef __OBJC__
#import "FIRFunctions.h"
#endif  // __OBJC__

// This defines the class FIRFunctionsPointer, which is a C++-compatible wrapper
// around the FIRFunctions Obj-C class.
OBJ_C_PTR_WRAPPER(FIRFunctions);

#pragma clang assume_nonnull begin

namespace firebase {
namespace functions {
namespace internal {

class FunctionsInternal {
 public:
  FunctionsInternal(App* app, const char* region);

  ~FunctionsInternal();

  // Get the firebase::App that this Functions was created with.
  App* app() const;

  // Return the region we were created with.
  const char* region() const;

  // Get a FunctionsReference for the specified path.
  HttpsCallableReferenceInternal* GetHttpsCallable(const char* name) const;

  void UseFunctionsEmulator(const char* origin);

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return true; }

  // When this is deleted, it will clean up all FunctionsReferences and other
  // objects.
  CleanupNotifier& cleanup() { return cleanup_; }

 private:
  // The firebase::App that this Functions was created with.
  App* app_;

  // The region that this Functions was created with.
  std::string region_;

  UniquePtr<FIRFunctionsPointer> impl_;

  FutureManager future_manager_;

  CleanupNotifier cleanup_;
};

#pragma clang assume_nonnull end

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_IOS_FUNCTIONS_IOS_H_
