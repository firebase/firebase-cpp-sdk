// Copyright 2026 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_RECAPTCHA_ENTERPRISE_PROVIDER_H_
#define FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_RECAPTCHA_ENTERPRISE_PROVIDER_H_

#include "firebase/app_check.h"

namespace firebase {
namespace app_check {

namespace internal {
class RecaptchaEnterpriseProviderFactoryInternal;
}

/// Implementation of an {@link AppCheckProviderFactory} that builds
/// RecaptchaEnterpriseProviders. This is the default implementation.
class RecaptchaEnterpriseProviderFactory : public AppCheckProviderFactory {
 public:
#if !defined(DOXYGEN)
  RecaptchaEnterpriseProviderFactory(
      const RecaptchaEnterpriseProviderFactory&) = delete;
  RecaptchaEnterpriseProviderFactory& operator=(
      const RecaptchaEnterpriseProviderFactory&) = delete;
#endif  // !defined(DOXYGEN)

  /// Gets an instance of this class for installation into a
  /// firebase::app_check::AppCheck instance.
  static RecaptchaEnterpriseProviderFactory* GetInstance();

  /// Gets the AppCheckProvider associated with the given
  /// {@link App} instance, or creates one if none
  /// already exists.
  firebase::app_check::AppCheckProvider* CreateProvider(App* app) override;

 private:
  RecaptchaEnterpriseProviderFactory();
  ~RecaptchaEnterpriseProviderFactory() override;

  internal::RecaptchaEnterpriseProviderFactoryInternal* internal_;
};

}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_RECAPTCHA_ENTERPRISE_PROVIDER_H_
