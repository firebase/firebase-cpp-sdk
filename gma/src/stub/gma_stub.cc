/*
 * Copyright 2021 Google LLC
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

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma.h"

namespace firebase {
namespace gma {

DEFINE_FIREBASE_VERSION_STRING(FirebaseGma);

static const firebase::App* g_app = nullptr;
static bool g_initialized = false;

// Constants representing each GMA function that returns a Future.
enum GmaFn {
  kGmaFnInitialize,
  kGmaFnCount,
};

static ReferenceCountedFutureImpl* g_future_impl = nullptr;

static Future<AdapterInitializationStatus> CreateAndCompleteInitializeStub() {
  FIREBASE_ASSERT(g_future_impl);

  // Return an AdapterInitializationStatus with one placeholder adapter.
  SafeFutureHandle<AdapterInitializationStatus> handle =
      g_future_impl->SafeAlloc<AdapterInitializationStatus>(kGmaFnInitialize);
  std::map<std::string, AdapterStatus> adapter_map;
  adapter_map["stub"] =
      GmaInternal::CreateAdapterStatus("stub adapter", true, 100);

  AdapterInitializationStatus adapter_init_status =
      GmaInternal::CreateAdapterInitializationStatus(adapter_map);
  g_future_impl->CompleteWithResult(handle, 0, "", adapter_init_status);
  return MakeFuture(g_future_impl, handle);
}

Future<AdapterInitializationStatus> Initialize(const ::firebase::App& app,
                                               InitResult* init_result_out) {
  FIREBASE_ASSERT(!g_initialized);
  g_future_impl = new ReferenceCountedFutureImpl(kGmaFnCount);

  (void)app;
  g_app = &app;
  g_initialized = true;
  RegisterTerminateOnDefaultAppDestroy();
  if (init_result_out) {
    *init_result_out = kInitResultSuccess;
  }
  return CreateAndCompleteInitializeStub();
}

Future<AdapterInitializationStatus> Initialize(InitResult* init_result_out) {
  FIREBASE_ASSERT(!g_initialized);
  g_future_impl = new ReferenceCountedFutureImpl(kGmaFnCount);

  g_initialized = true;
  g_app = nullptr;
  RegisterTerminateOnDefaultAppDestroy();
  if (init_result_out) {
    *init_result_out = kInitResultSuccess;
  }
  return CreateAndCompleteInitializeStub();
}

Future<AdapterInitializationStatus> InitializeLastResult() {
  return g_future_impl
             ? static_cast<const Future<AdapterInitializationStatus>&>(
                   g_future_impl->LastResult(kGmaFnInitialize))
             : Future<AdapterInitializationStatus>();
}

AdapterInitializationStatus GetInitializationStatus() {
  Future<AdapterInitializationStatus> result = InitializeLastResult();
  if (result.status() != firebase::kFutureStatusComplete) {
    return GmaInternal::CreateAdapterInitializationStatus({});
  } else {
    return *result.result();
  }
}

void DisableSDKCrashReporting() {}

void DisableMediationInitialization() {}

bool IsInitialized() { return g_initialized; }

void SetRequestConfiguration(
    const RequestConfiguration& request_configuration) {}

RequestConfiguration GetRequestConfiguration() {
  return RequestConfiguration();
}

void OpenAdInspector(AdParent parent, AdInspectorClosedListener* listener) {}
void SetIsSameAppKeyEnabled(bool is_enabled) {}

void Terminate() {
  FIREBASE_ASSERT(g_initialized);

  delete g_future_impl;
  g_future_impl = nullptr;

  UnregisterTerminateOnDefaultAppDestroy();
  DestroyCleanupNotifier();
  g_initialized = false;
  g_app = nullptr;
}

const ::firebase::App* GetApp() { return g_app; }

}  // namespace gma
}  // namespace firebase
