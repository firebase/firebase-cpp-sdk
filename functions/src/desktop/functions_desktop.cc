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
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "functions/src/desktop/callable_reference_desktop.h"

namespace firebase {
namespace functions {
namespace internal {

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : app_(app), region_(region) {}

FunctionsInternal::~FunctionsInternal() {}

::firebase::App* FunctionsInternal::app() const { return app_; }

const char* FunctionsInternal::region() const { return region_.c_str(); }

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    const char* name) const {
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), name);
}

std::string FunctionsInternal::GetUrl(const std::string& name) const {
  std::string proj = app_->options().project_id();
  std::string url =
      emulator_origin_.empty()
          ? "https://" + region_ + "-" + proj + ".cloudfunctions.net/" + name
          : emulator_origin_ + "/" + proj + "/" + region_ + "/" + name;
  return url;
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  emulator_origin_ = origin;
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
