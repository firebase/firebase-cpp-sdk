/*
 * Copyright 2016 Google LLC
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

#include "admob/src/include/firebase/admob.h"

#import <GoogleMobileAds/GoogleMobileAds.h>

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/include/firebase/app.h"
#include "app/src/log.h"

namespace firebase {
namespace admob {

static const ::firebase::App* g_app = nullptr;

static bool g_initialized = false;

InitResult Initialize() {
  g_initialized = true;
  RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

InitResult Initialize(const char* admob_app_id) {
  if (g_initialized) {
    LogWarning("AdMob is already initialized.");
    return kInitResultSuccess;
  }
  g_initialized = true;
#if 0  // This currently crashes. b/32668185
  if (admob_app_id) {
    [GADMobileAds configureWithApplicationID:@(admob_app_id)];
  }
#endif
  RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

InitResult Initialize(const ::firebase::App& app) { return Initialize(app, nullptr); }

InitResult Initialize(const ::firebase::App& app, const char* admob_app_id) {
  if (g_initialized) {
    LogWarning("AdMob is already initialized.");
    return kInitResultSuccess;
  }
  g_initialized = true;
  g_app = &app;
#if 0  // This currently crashes. b/32668185
  if (admob_app_id) {
    [GADMobileAds configureWithApplicationID:@(admob_app_id)];
  }
#endif
  RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

bool IsInitialized() { return g_initialized; }

void Terminate() {
  UnregisterTerminateOnDefaultAppDestroy();
  DestroyCleanupNotifier();
  g_initialized = false;
  g_app = nullptr;
}

const ::firebase::App* GetApp() { return g_app; }

}  // namespace admob
}  // namespace firebase
