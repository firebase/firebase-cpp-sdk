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

#include "app/src/app_state.h"
#include "app/src/future_manager.h"
#include "functions/src/desktop/callable_reference_desktop.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

namespace firebase {
namespace functions {
namespace internal {

class FunctionsInternal : public ::firebase::internal::Service {
 public:
  FunctionsInternal(::firebase::internal::AppState& app_state, const App& app,
                    const char* region);

  ~FunctionsInternal() override;

  // Get the firebase::App that this Functions was created with.
  App app() const;

  ::firebase::internal::AppState& app_state() const { return app_state_; }

  // Get the region that this Functions was created with.
  const char* region() const;

  // Get a FunctionsReference for the specified path.
  HttpsCallableReferenceInternal* GetHttpsCallable(
      std::shared_ptr<FunctionsInternal> self, const char* name) const;

  HttpsCallableReferenceInternal* GetHttpsCallable(
      std::shared_ptr<FunctionsInternal> self, const char* name,
      const HttpsCallableOptions& options) const;

  // Get a FunctionsReference for the specified URL.
  HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      std::shared_ptr<FunctionsInternal> self, const char* url) const;

  HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      std::shared_ptr<FunctionsInternal> self, const char* url,
      const HttpsCallableOptions& options) const;

  void UseFunctionsEmulator(const char* origin);

  // Returns the URL for the endpoint with the given name.
  std::string GetUrl(const std::string& name) const;

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return true; }

 private:
  ::firebase::internal::AppState& app_state_;
  std::string region_;

  // In non-empty, the origin to use for constructing emulator functions URLs.
  std::string emulator_origin_;

  FutureManager future_manager_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_DESKTOP_FUNCTIONS_DESKTOP_H_
