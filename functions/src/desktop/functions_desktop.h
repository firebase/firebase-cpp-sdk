// Copyright 2018 Google LLC
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

#ifndef FIREBASE_FUNCTIONS_SRC_DESKTOP_FUNCTIONS_DESKTOP_H_
#define FIREBASE_FUNCTIONS_SRC_DESKTOP_FUNCTIONS_DESKTOP_H_

#include <memory>
#include <string>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

namespace firebase {
namespace functions {
namespace internal {

// Shared lifetime token allowing asynchronous callbacks to check whether
// FunctionsInternal (and its FutureManager) is still alive under a mutex,
// and deferring destruction of completed call contexts until outside
// Response::MarkCompleted().
struct FunctionsLifetimeState {
  Mutex mutex;
  bool alive = true;
  std::vector<std::shared_ptr<void>> retired_calls;
};

class FunctionsInternal {
 public:
  FunctionsInternal(App* app, const char* region);
  ~FunctionsInternal();

  FunctionsInternal(const FunctionsInternal&) = delete;
  FunctionsInternal& operator=(const FunctionsInternal&) = delete;

  App* app() const;
  const char* region() const;

  HttpsCallableReferenceInternal* GetHttpsCallable(const char* name) const;
  HttpsCallableReferenceInternal* GetHttpsCallable(
      const char* name, const HttpsCallableOptions& options) const {
    return GetHttpsCallableWithOptions(name, options);
  }
  HttpsCallableReferenceInternal* GetHttpsCallableWithOptions(
      const char* name, const HttpsCallableOptions& options) const;
  HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      const char* url) const;
  HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      const char* url, const HttpsCallableOptions& options) const {
    return GetHttpsCallableFromURLWithOptions(url, options);
  }
  HttpsCallableReferenceInternal* GetHttpsCallableFromURLWithOptions(
      const char* url, const HttpsCallableOptions& options) const;
  void UseFunctionsEmulator(const char* origin);

  // Whether this object was successfully initialized by the constructor.
  bool is_valid() const;
  bool initialized() const { return is_valid(); }

  // Compute the URL for the given function trigger name.
  std::string GetUrl(const std::string& name) const;

  FutureManager& future_manager() { return future_manager_; }

  CleanupNotifier& cleanup() { return cleanup_; }

  const std::shared_ptr<FunctionsLifetimeState>& lifetime_state() const {
    return lifetime_state_;
  }

 private:
  App* app_;
  std::string region_;
  mutable Mutex mutex_;
  std::string emulator_origin_;
  FutureManager future_manager_;
  CleanupNotifier cleanup_;
  std::shared_ptr<FunctionsLifetimeState> lifetime_state_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_DESKTOP_FUNCTIONS_DESKTOP_H_
