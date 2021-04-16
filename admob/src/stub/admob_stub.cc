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

#include <cstdarg>
#include <cstddef>

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"

namespace firebase {
namespace admob {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAdMob);

static const firebase::App* g_app = nullptr;
static bool g_initialized = false;

InitResult Initialize(const ::firebase::App& app) {
  (void)app;
  g_app = &app;
  g_initialized = true;
  RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

InitResult Initialize(const ::firebase::App& app, const char* admob_app_id) {
  (void)app;
  (void)admob_app_id;
  g_app = &app;
  g_initialized = true;
  RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

InitResult Initialize() {
  g_initialized = true;
  g_app = nullptr;
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
