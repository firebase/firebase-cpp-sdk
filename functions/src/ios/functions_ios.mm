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

#import "FirebaseFunctions-Swift.h"

#include <charconv>
#include <memory>
#include <string>

#include "app/src/app_ios.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"
#include "functions/src/ios/callable_reference_ios.h"

namespace firebase {
namespace functions {
namespace internal {

namespace {
bool HasHttpScheme(const std::string& s) {
  return s.compare(0, 7, "http://") == 0 || s.compare(0, 8, "https://") == 0;
}

void ApplyCallableTimeout(FIRHTTPSCallable* callable,
                          const HttpsCallableOptions& options) {
  if (callable && options.timeout_ms > 0 &&
      options.timeout_ms != kDefaultTimeoutMilliseconds) {
    callable.timeoutInterval =
        static_cast<NSTimeInterval>(options.timeout_ms) / 1000.0;
  }
}
}  // namespace

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : app_(app),
      region_(region ? region : "us-central1"),
      lifetime_state_(std::make_shared<FunctionsLifetimeState>()) {
  FIRFunctions* fir_functions = nil;
  if (app_ && app_->data_) {
    if (HasHttpScheme(region_)) {
      fir_functions =
          [FIRFunctions functionsForApp:app_->data_->GetPlatformApp()
                           customDomain:@(region_.c_str())];
    } else {
      fir_functions =
          [FIRFunctions functionsForApp:app_->data_->GetPlatformApp()
                                 region:@(region_.c_str())];
    }
  }
  impl_.reset(new FIRFunctionsPointer(fir_functions));
}

FunctionsInternal::~FunctionsInternal() {
  MutexLock lock(lifetime_state_->mutex);
  lifetime_state_->alive = false;
}

bool FunctionsInternal::is_valid() const {
  return app_ != nullptr && impl_ && impl() != nil;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    const char* name) const {
  if (!is_valid() || !name || name[0] == '\0') return nullptr;
  FIRHTTPSCallable* callable = [impl() HTTPSCallableWithName:@(name)];
  if (!callable) return nullptr;
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this),
      std::unique_ptr<FIRHTTPSCallablePointer>(
          new FIRHTTPSCallablePointer(callable)));
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableWithOptions(
    const char* name, const HttpsCallableOptions& options) const {
  if (!is_valid() || !name || name[0] == '\0') return nullptr;
  FIRHTTPSCallableOptions* objc_options = [[FIRHTTPSCallableOptions alloc]
      initWithRequireLimitedUseAppCheckTokens:options
                                                  .limited_use_app_check_tokens];
  FIRHTTPSCallable* callable = [impl() HTTPSCallableWithName:@(name)
                                                     options:objc_options];
  if (!callable) return nullptr;
  ApplyCallableTimeout(callable, options);
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this),
      std::unique_ptr<FIRHTTPSCallablePointer>(
          new FIRHTTPSCallablePointer(callable)));
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableFromURL(
    const char* url) const {
  if (!is_valid() || !url || url[0] == '\0') return nullptr;
  NSURL* nsurl = [NSURL URLWithString:@(url)];
  if (!nsurl || !nsurl.scheme) return nullptr;
  FIRHTTPSCallable* callable = [impl() HTTPSCallableWithURL:nsurl];
  if (!callable) return nullptr;
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this),
      std::unique_ptr<FIRHTTPSCallablePointer>(
          new FIRHTTPSCallablePointer(callable)));
}

HttpsCallableReferenceInternal*
FunctionsInternal::GetHttpsCallableFromURLWithOptions(
    const char* url, const HttpsCallableOptions& options) const {
  if (!is_valid() || !url || url[0] == '\0') return nullptr;
  NSURL* nsurl = [NSURL URLWithString:@(url)];
  if (!nsurl || !nsurl.scheme) return nullptr;
  FIRHTTPSCallableOptions* objc_options = [[FIRHTTPSCallableOptions alloc]
      initWithRequireLimitedUseAppCheckTokens:options
                                                  .limited_use_app_check_tokens];
  FIRHTTPSCallable* callable = [impl() HTTPSCallableWithURL:nsurl
                                                    options:objc_options];
  if (!callable) return nullptr;
  ApplyCallableTimeout(callable, options);
  return new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this),
      std::unique_ptr<FIRHTTPSCallablePointer>(
          new FIRHTTPSCallablePointer(callable)));
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  if (!is_valid() || !origin || origin[0] == '\0') return;
  std::string str(origin);
  if (str.compare(0, 7, "http://") == 0) {
    str.erase(0, 7);
  } else if (str.compare(0, 8, "https://") == 0) {
    str.erase(0, 8);
  }
  while (!str.empty() && str.back() == '/') {
    str.pop_back();
  }
  size_t pos = str.rfind(':');
  if (pos == std::string::npos || pos == 0 || pos + 1 >= str.size()) {
    LogError("Invalid emulator origin: %s", origin);
    return;
  }
  int port = 0;
  const char* p_begin = str.data() + pos + 1;
  const char* p_end = str.data() + str.size();
  auto [ptr, ec] = std::from_chars(p_begin, p_end, port);
  if (ec != std::errc() || ptr != p_end || port <= 0 || port > 65535) {
    LogError("Invalid emulator port in origin: %s", origin);
    return;
  }
  std::string host = str.substr(0, pos);
  [impl() useEmulatorWithHost:@(host.c_str()) port:port];
}

Error FunctionsInternal::ErrorFromNSError(NSError* error) {
  if (error == nil) {
    return kErrorNone;
  }
  if ([error.domain isEqualToString:NSURLErrorDomain]) {
    if (error.code == NSURLErrorTimedOut) {
      return kErrorDeadlineExceeded;
    }
    if (error.code == NSURLErrorCancelled) {
      return kErrorCancelled;
    }
    return kErrorUnavailable;
  }
  switch ((FIRFunctionsErrorCode)error.code) {
    case FIRFunctionsErrorCodeOK:
      return kErrorInternal;
    case FIRFunctionsErrorCodeCancelled:
      return kErrorCancelled;
    case FIRFunctionsErrorCodeUnknown:
      return kErrorUnknown;
    case FIRFunctionsErrorCodeInvalidArgument:
      return kErrorInvalidArgument;
    case FIRFunctionsErrorCodeDeadlineExceeded:
      return kErrorDeadlineExceeded;
    case FIRFunctionsErrorCodeNotFound:
      return kErrorNotFound;
    case FIRFunctionsErrorCodeAlreadyExists:
      return kErrorAlreadyExists;
    case FIRFunctionsErrorCodePermissionDenied:
      return kErrorPermissionDenied;
    case FIRFunctionsErrorCodeUnauthenticated:
      return kErrorUnauthenticated;
    case FIRFunctionsErrorCodeResourceExhausted:
      return kErrorResourceExhausted;
    case FIRFunctionsErrorCodeFailedPrecondition:
      return kErrorFailedPrecondition;
    case FIRFunctionsErrorCodeAborted:
      return kErrorAborted;
    case FIRFunctionsErrorCodeOutOfRange:
      return kErrorOutOfRange;
    case FIRFunctionsErrorCodeUnimplemented:
      return kErrorUnimplemented;
    case FIRFunctionsErrorCodeInternal:
      return kErrorInternal;
    case FIRFunctionsErrorCodeUnavailable:
      return kErrorUnavailable;
    case FIRFunctionsErrorCodeDataLoss:
      return kErrorDataLoss;
    default:
      return kErrorInternal;
  }
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
