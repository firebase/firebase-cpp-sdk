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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

namespace firebase {
namespace functions {
namespace internal {

struct FunctionsLifetimeState {
  std::mutex mutex;
  std::condition_variable cv;
  bool alive = true;
  std::vector<std::thread::id> resolving_threads;
};

class FunctionsInternal {
 public:
  FunctionsInternal(App* app, const char* region);
  ~FunctionsInternal();

  FunctionsInternal(const FunctionsInternal&) = delete;
  FunctionsInternal& operator=(const FunctionsInternal&) = delete;

  App* app() const { return app_; }
  const char* region() const { return region_.c_str(); }
  bool is_valid() const { return app_ != nullptr; }
  bool initialized() const { return is_valid(); }

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

  std::string GetUrl(const std::string& name) const;

  const std::shared_ptr<FunctionsLifetimeState>& lifetime_state() const {
    return lifetime_state_;
  }
  ReferenceCountedFutureImpl* future() { return &future_impl_; }

 private:
  App* app_;
  std::string region_;
  mutable Mutex mutex_;
  std::string emulator_origin_;
  std::shared_ptr<FunctionsLifetimeState> lifetime_state_;
  ReferenceCountedFutureImpl future_impl_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_DESKTOP_FUNCTIONS_DESKTOP_H_
