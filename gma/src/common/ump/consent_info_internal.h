/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_

#include "app/src/cleanup_notifier.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/future.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

// Constants representing each ConsentInfo function that returns a Future.
enum ConsentInfoFn {
  kConsentInfoFnRequestConsentStatus,
  kConsentInfoFnLoadConsentForm,
  kConsentInfoFnShowConsentForm,
  kConsentInfoFnCount
};

class ConsentInfoInternal {
 public:
  virtual ~ConsentInfoInternal();

  // Implemented in platform-specific code to instantiate a
  // platform-specific subclass.
  static ConsentInfoInternal* CreateInstance();

 protected:
  ConsentInfoInternal();

  ReferenceCountedFutureImpl* futures() { return &futures_; }
  CleanupNotifier* cleanup() { return &cleanup_; }

 private:
  ReferenceCountedFutureImpl futures_;
  CleanupNotifier cleanup_;
};

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_
