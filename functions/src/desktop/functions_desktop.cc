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

#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/rest/transport_curl.h"
#include "app/src/assert.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/log.h"
#include "functions/src/desktop/callable_reference_desktop.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

namespace firebase::functions::internal {
namespace {
bool HasHttpScheme(std::string_view s) {
  return s.substr(0, 7) == "http://" || s.substr(0, 8) == "https://";
}
std::string StripTrailingSlashes(std::string_view s) {
  while (!s.empty() && s.back() == '/') s.remove_suffix(1);
  return std::string(s);
}
std::string StripLeadingSlashes(std::string_view s) {
  while (!s.empty() && s.front() == '/') s.remove_prefix(1);
  return std::string(s);
}
}  // namespace

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : app_(app),
      region_(region ? StripTrailingSlashes(region) : "us-central1"),
      lifetime_state_(std::make_shared<FunctionsLifetimeState>()),
      future_impl_(1) {
  FIREBASE_ASSERT(app != nullptr);
  rest::InitTransportCurl();
  if (app_) {
    app_->function_registry()->CallFunction(
        ::firebase::internal::FnAuthStartTokenListener, app_, nullptr, nullptr);
  }
}

FunctionsInternal::~FunctionsInternal() {
  {
    std::unique_lock<std::mutex> lock(lifetime_state_->mutex);
    lifetime_state_->alive = false;
    const std::thread::id self_id = std::this_thread::get_id();
    lifetime_state_->cv.wait(lock, [this, self_id]() {
      for (const std::thread::id& id : lifetime_state_->resolving_threads) {
        if (id != self_id) return false;
      }
      return true;
    });
  }
  if (app_) {
    app_->function_registry()->CallFunction(
        ::firebase::internal::FnAuthStopTokenListener, app_, nullptr, nullptr);
  }
  rest::CleanupTransportCurl();
  app_ = nullptr;
}

std::string FunctionsInternal::GetUrl(const std::string& name) const {
  std::string clean_name = StripLeadingSlashes(name);
  if (clean_name.empty()) return "";

  std::string origin;
  {
    MutexLock lock(mutex_);
    origin = emulator_origin_;
  }

  const char* proj_str = app_ ? app_->options().project_id() : nullptr;
  const std::string proj = proj_str ? proj_str : "";

  if (!origin.empty()) {
    if (proj.empty()) {
      LogError("Functions: project_id is required when using the emulator.");
      return "";
    }
    const std::string effective_region =
        HasHttpScheme(region_) ? "us-central1" : region_;
    return origin + "/" + proj + "/" + effective_region + "/" + clean_name;
  }
  if (HasHttpScheme(region_)) return region_ + "/" + clean_name;
  if (proj.empty() || region_.empty()) {
    LogError("Functions: project_id and region are required to construct URL.");
    return "";
  }
  return "https://" + region_ + "-" + proj + ".cloudfunctions.net/" +
         clean_name;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    const char* name) const {
  return GetHttpsCallableWithOptions(name, HttpsCallableOptions());
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableWithOptions(
    const char* name, const HttpsCallableOptions& options) const {
  bool empty = !name || name[0] == '\0';
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), empty ? "" : name,
      /*is_url=*/empty, options);
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableFromURL(
    const char* url) const {
  return GetHttpsCallableFromURLWithOptions(url, HttpsCallableOptions());
}

HttpsCallableReferenceInternal*
FunctionsInternal::GetHttpsCallableFromURLWithOptions(
    const char* url, const HttpsCallableOptions& options) const {
  const char* valid_url = (url && HasHttpScheme(url)) ? url : "";
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), valid_url, /*is_url=*/true,
      options);
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  MutexLock lock(mutex_);
  if (!origin || origin[0] == '\0') {
    emulator_origin_.clear();
    return;
  }
  std::string formatted(origin);
  if (!HasHttpScheme(formatted)) formatted = "http://" + formatted;
  emulator_origin_ = StripTrailingSlashes(formatted);
}

}  // namespace firebase::functions::internal
