// Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_DEBUG_PROVIDER_H_
#define FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_DEBUG_PROVIDER_H_

#include "firebase/app_check.h"

namespace firebase {
namespace app_check {

namespace internal {
class DebugAppCheckProviderFactoryInternal;
}

/// Implementation of an {@link AppCheckProviderFactory} that builds
/// DebugAppCheckProviders.
///
/// DebugAppCheckProvider can exchange a debug token registered in the Firebase
/// console for a Firebase App Check token. The debug provider is designed to
/// enable testing applications on a simulator or test environment.
///
/// NOTE: Do not use the debug provider in applications used by real users.
class DebugAppCheckProviderFactory : public AppCheckProviderFactory {
 public:
  ~DebugAppCheckProviderFactory() override;

  /// Gets an instance of this class for installation into a
  /// firebase::app_check::AppCheck instance.
  static DebugAppCheckProviderFactory* GetInstance();

  /// Gets the AppCheckProvider associated with the given
  /// {@link App} instance, or creates one if none
  /// already exists.
  AppCheckProvider* CreateProvider(App* app) override;

 private:
  DebugAppCheckProviderFactory();

  internal::DebugAppCheckProviderFactoryInternal* internal_;
};

}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_DEBUG_PROVIDER_H_
