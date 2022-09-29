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

#include "firebase/app_check/debug_provider.h"

namespace firebase {
namespace app_check {

static DebugAppCheckProviderFactory* g_debug_app_check_provider_factory =
    nullptr;

DebugAppCheckProviderFactory* DebugAppCheckProviderFactory::GetInstance() {
  if (!g_debug_app_check_provider_factory) {
    g_debug_app_check_provider_factory = new DebugAppCheckProviderFactory();
  }
  return g_debug_app_check_provider_factory;
}

DebugAppCheckProviderFactory::DebugAppCheckProviderFactory() {}

DebugAppCheckProviderFactory::~DebugAppCheckProviderFactory() {}

AppCheckProvider* DebugAppCheckProviderFactory::CreateProvider(App* app) {
  return nullptr;
}

}  // namespace app_check
}  // namespace firebase
