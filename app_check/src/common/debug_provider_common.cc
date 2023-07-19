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

#include <string>

#include "firebase/app_check/debug_provider.h"

// Include the header that matches the platform being used.
#if FIREBASE_PLATFORM_ANDROID
#include "app_check/src/android/debug_provider_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "app_check/src/ios/debug_provider_ios.h"
#else
#include "app_check/src/desktop/debug_provider_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace app_check {

DebugAppCheckProviderFactory* DebugAppCheckProviderFactory::GetInstance() {
  static DebugAppCheckProviderFactory g_debug_app_check_provider_factory;
  return &g_debug_app_check_provider_factory;
}

DebugAppCheckProviderFactory::DebugAppCheckProviderFactory()
    : internal_(new internal::DebugAppCheckProviderFactoryInternal()) {}

DebugAppCheckProviderFactory::~DebugAppCheckProviderFactory() {
  if (internal_) {
    delete internal_;
    internal_ = nullptr;
  }
}

AppCheckProvider* DebugAppCheckProviderFactory::CreateProvider(App* app) {
  if (internal_) {
    return internal_->CreateProvider(app);
  }
  return nullptr;
}

void DebugAppCheckProviderFactory::SetDebugToken(const std::string& token) {
  if (internal_) {
    return internal_->SetDebugToken(token);
  }
}

}  // namespace app_check
}  // namespace firebase
