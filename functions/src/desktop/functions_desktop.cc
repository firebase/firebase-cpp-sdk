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

#include "functions/src/desktop/functions_desktop.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "app/rest/transport_curl.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "functions/src/desktop/callable_reference_desktop.h"

namespace firebase {
namespace functions {
namespace internal {

FunctionsInternal::FunctionsInternal(::firebase::internal::AppState& app_state,
                                     const App& /*app*/, const char* region)
    : app_state_(app_state), region_(region) {
  rest::InitTransportCurl();
}

FunctionsInternal::~FunctionsInternal() {
  std::vector<std::shared_ptr<CallOperation>> remaining;
  {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    remaining.swap(active_operations_);
  }
  for (auto& op : remaining) {
    op->CancelAndWait();
  }
  remaining.clear();
  rest::CleanupTransportCurl();
}

void FunctionsInternal::AddOperation(std::shared_ptr<CallOperation> op) {
  std::vector<std::shared_ptr<CallOperation>> completed;
  {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    for (auto it = active_operations_.begin();
         it != active_operations_.end();) {
      if ((*it)->is_completed()) {
        completed.push_back(std::move(*it));
        it = active_operations_.erase(it);
      } else {
        ++it;
      }
    }
    if (op) {
      active_operations_.push_back(std::move(op));
    }
  }
  // Destroy completed operations outside `operations_mutex_`.
  completed.clear();
}

::firebase::App FunctionsInternal::app() const {
  return app_state_.TryMakeAppHandle();
}

const char* FunctionsInternal::region() const { return region_.c_str(); }

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    std::shared_ptr<FunctionsInternal> self, const char* name) const {
  return GetHttpsCallable(std::move(self), name, HttpsCallableOptions());
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    std::shared_ptr<FunctionsInternal> self, const char* name,
    const HttpsCallableOptions& options) const {
  return new HttpsCallableReferenceInternal(std::move(self),
                                            GetUrl(name).c_str(), options);
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableFromURL(
    std::shared_ptr<FunctionsInternal> self, const char* url) const {
  return GetHttpsCallableFromURL(std::move(self), url, HttpsCallableOptions());
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableFromURL(
    std::shared_ptr<FunctionsInternal> self, const char* url,
    const HttpsCallableOptions& options) const {
  return new HttpsCallableReferenceInternal(std::move(self), url, options);
}

std::string FunctionsInternal::GetUrl(const std::string& name) const {
  std::string proj = app_state_.options().project_id();
  std::string url =
      emulator_origin_.empty()
          ? "https://" + region_ + "-" + proj + ".cloudfunctions.net/" + name
          : emulator_origin_ + "/" + proj + "/" + region_ + "/" + name;
  return url;
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  emulator_origin_ = origin ? origin : "";
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
