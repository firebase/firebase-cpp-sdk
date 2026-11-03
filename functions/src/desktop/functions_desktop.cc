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

namespace firebase {
namespace functions {
namespace internal {

namespace {
bool HasHttpScheme(std::string_view s) {
  return s.substr(0, 7) == "http://" || s.substr(0, 8) == "https://";
}

std::string StripTrailingSlashes(std::string_view s) {
  while (!s.empty() && s.back() == '/') {
    s.remove_suffix(1);
  }
  return std::string(s);
}

std::string StripLeadingSlashes(std::string_view s) {
  while (!s.empty() && s.front() == '/') {
    s.remove_prefix(1);
  }
  return std::string(s);
}
}  // namespace

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : app_(app),
      region_(region ? StripTrailingSlashes(region) : "us-central1"),
      lifetime_state_(std::make_shared<FunctionsLifetimeState>()) {
  FIREBASE_ASSERT(app != nullptr);
  rest::InitTransportCurl();
  if (app_) {
    app_->function_registry()->CallFunction(
        ::firebase::internal::FnAuthStartTokenListener, app_, nullptr, nullptr);
  }
}

FunctionsInternal::~FunctionsInternal() {
  {
    MutexLock lock(lifetime_state_->mutex);
    lifetime_state_->alive = false;
  }
  if (app_) {
    app_->function_registry()->CallFunction(
        ::firebase::internal::FnAuthStopTokenListener, app_, nullptr, nullptr);
  }
  rest::CleanupTransportCurl();
  {
    std::vector<std::shared_ptr<void>> to_destroy;
    {
      MutexLock lock(lifetime_state_->mutex);
      to_destroy.swap(lifetime_state_->retired_calls);
    }
  }
  app_ = nullptr;
}

App* FunctionsInternal::app() const { return app_; }

const char* FunctionsInternal::region() const { return region_.c_str(); }

bool FunctionsInternal::is_valid() const { return app_ != nullptr; }

std::string FunctionsInternal::GetUrl(const std::string& name) const {
  std::string clean_name = StripLeadingSlashes(name);
  if (clean_name.empty()) {
    return "";
  }

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
    // If region_ was a custom domain URL, fall back to "us-central1" for the
    // emulator path segment.
    const std::string effective_region =
        HasHttpScheme(region_) ? "us-central1" : region_;
    return origin + "/" + proj + "/" + effective_region + "/" + clean_name;
  }

  if (HasHttpScheme(region_)) {
    return region_ + "/" + clean_name;
  }

  if (proj.empty() || region_.empty()) {
    LogError("Functions: project_id and region are required to construct URL.");
    return "";
  }
  return std::string("https://") + region_ + "-" + proj +
         ".cloudfunctions.net/" + clean_name;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    const char* name) const {
  if (!name || name[0] == '\0') {
    return new HttpsCallableReferenceInternal(
        const_cast<FunctionsInternal*>(this), "", /*is_url=*/true);
  }
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), name, /*is_url=*/false);
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableWithOptions(
    const char* name, const HttpsCallableOptions& options) const {
  if (!name || name[0] == '\0') {
    return new HttpsCallableReferenceInternal(
        const_cast<FunctionsInternal*>(this), "", /*is_url=*/true, options);
  }
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), name, /*is_url=*/false, options);
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableFromURL(
    const char* url) const {
  std::string valid_url = (url && HasHttpScheme(url)) ? url : "";
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), valid_url.c_str(), /*is_url=*/true);
}

HttpsCallableReferenceInternal*
FunctionsInternal::GetHttpsCallableFromURLWithOptions(
    const char* url, const HttpsCallableOptions& options) const {
  std::string valid_url = (url && HasHttpScheme(url)) ? url : "";
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), valid_url.c_str(), /*is_url=*/true,
      options);
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  MutexLock lock(mutex_);
  if (!origin || origin[0] == '\0') {
    emulator_origin_.clear();
    return;
  }
  std::string formatted(origin);
  if (!HasHttpScheme(formatted)) {
    formatted = std::string("http://") + formatted;
  }
  emulator_origin_ = StripTrailingSlashes(formatted);
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
