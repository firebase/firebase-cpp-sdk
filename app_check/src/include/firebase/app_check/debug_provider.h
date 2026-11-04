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

#include <string>

#include "firebase/app_check.h"

namespace firebase {
namespace app_check {

namespace internal {
class DebugAppCheckProviderFactoryInternal;
}

class DebugAppCheckProviderFactory : public AppCheckProviderFactory {
 public:
#if !defined(DOXYGEN)
  DebugAppCheckProviderFactory(const DebugAppCheckProviderFactory&) = delete;
  DebugAppCheckProviderFactory& operator=(const DebugAppCheckProviderFactory&) =
      delete;
#endif  // !defined(DOXYGEN)

#if !defined(SWIG)
  [[nodiscard]]
#endif
  static DebugAppCheckProviderFactory* GetInstance();

#if !defined(SWIG)
  [[nodiscard]]
#endif
  firebase::app_check::AppCheckProvider* CreateProvider(App* app) override;

  void SetDebugToken(const char* token);
  void SetDebugToken(const std::string& token);

 private:
  DebugAppCheckProviderFactory();
  ~DebugAppCheckProviderFactory() override;

  internal::DebugAppCheckProviderFactoryInternal* internal_{nullptr};
};

}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_DEBUG_PROVIDER_H_
