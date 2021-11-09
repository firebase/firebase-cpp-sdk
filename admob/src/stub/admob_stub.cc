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
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/version.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAdMob);

static const firebase::App* g_app = nullptr;
static bool g_initialized = false;

// Constants representing each AdMob function that returns a Future.
enum AdMobFn {
  kAdMobFnInitialize,
  kAdMobFnCount,
};

static ReferenceCountedFutureImpl* g_future_impl = nullptr;

static Future<AdapterInitializationStatus> CreateAndCompleteInitializeStub() {
  FIREBASE_ASSERT(g_future_impl);

  // Return an AdapterInitializationStatus with one placeholder adapter.
  SafeFutureHandle<AdapterInitializationStatus> handle =
      g_future_impl->SafeAlloc<AdapterInitializationStatus>(kAdMobFnInitialize);
  std::map<std::string, AdapterStatus> adapter_map;
  adapter_map["stub"] =
      AdMobInternal::CreateAdapterStatus("stub adapter", true, 100);

  AdapterInitializationStatus adapter_init_status =
      AdMobInternal::CreateAdapterInitializationStatus(std::move(adapter_map));
  g_future_impl->CompleteWithResult(handle, 0, "", adapter_init_status);
  return MakeFuture(g_future_impl, handle);
}

Future<AdapterInitializationStatus> Initialize(const ::firebase::App& app,
                                               InitResult* init_result_out) {
  FIREBASE_ASSERT(!g_initialized);
  g_future_impl = new ReferenceCountedFutureImpl(kAdMobFnCount);

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
  g_future_impl = new ReferenceCountedFutureImpl(kAdMobFnCount);

  g_initialized = true;
  g_app = nullptr;
  RegisterTerminateOnDefaultAppDestroy();
  if (init_result_out) {
    *init_result_out = kInitResultSuccess;
  }
  return CreateAndCompleteInitializeStub();
}

Future<AdapterInitializationStatus> InitializeLastResult() {
  return g_future_impl ? static_cast<const Future<AdapterInitializationStatus>&>(
                            g_future_impl->LastResult(kAdMobFnInitialize))
                      : Future<AdapterInitializationStatus>();
}

AdapterInitializationStatus GetInitializationStatus() {
  Future<AdapterInitializationStatus> result = InitializeLastResult();
  if (result.status() != firebase::kFutureStatusComplete) {
    return AdMobInternal::CreateAdapterInitializationStatus({});
  } else {
    return *result.result();
  }
}

bool IsInitialized() { return g_initialized; }

void SetRequestConfiguration(
    const RequestConfiguration& request_configuration) {}

RequestConfiguration GetRequestConfiguration() {
  return RequestConfiguration();
}

void Terminate() {
  delete g_future_impl;
  g_future_impl = nullptr;

  UnregisterTerminateOnDefaultAppDestroy();
  DestroyCleanupNotifier();
  g_initialized = false;
  g_app = nullptr;
}

const ::firebase::App* GetApp() { return g_app; }

}  // namespace admob
}  // namespace firebase
