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

#include "functions/src/ios/functions_ios.h"

#import "FIRFunctions.h"
#include "app/memory/unique_ptr.h"
#include "app/src/app_ios.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "functions/src/ios/callable_reference_ios.h"

namespace firebase {
namespace functions {
namespace internal {

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : app_(app), region_(region) {
  impl_.reset(new FIRFunctionsPointer([FIRFunctions
      functionsForApp:app->GetPlatformApp()
               region:@(region)]));
}

FunctionsInternal::~FunctionsInternal() {
  // Destructor is necessary for ARC garbage collection.
}

::firebase::App* FunctionsInternal::app() const { return app_; }
const char* FunctionsInternal::region() const { return region_.c_str(); }

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(const char* name) const {
  // HttpsCallableReferenceInternal handles deleting the wrapper pointer.
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this),
      MakeUnique<FIRHTTPSCallablePointer>([impl_.get()->get() HTTPSCallableWithName:@(name)]));
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  [impl_.get()->get() useFunctionsEmulatorOrigin:@(origin)];
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
