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

#include "admob/src/common/admob_common.h"

#include <assert.h>

#include <vector>

#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/banner_view.h"
#include "admob/src/include/firebase/admob/interstitial_ad.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/version.h"
#include "app/src/util.h"

FIREBASE_APP_REGISTER_CALLBACKS(
    admob,
    {
      if (app == ::firebase::App::GetInstance()) {
        return firebase::admob::Initialize(*app);
      }
      return kInitResultSuccess;
    },
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::admob::Terminate();
      }
    });

namespace firebase {
namespace admob {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAdMob);

static CleanupNotifier* g_cleanup_notifier = nullptr;
const char kAdMobModuleName[] = "admob";

void RegisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kAdMobModuleName)) {
    // It's possible to initialize AdMob without firebase::App so only register
    // for cleanup notifications if the default app exists.
    App* app = App::GetInstance();
    if (app) {
      CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(app);
      assert(cleanup_notifier);
      cleanup_notifier->RegisterObject(const_cast<char*>(kAdMobModuleName),
                                       [](void*) {
                                         if (firebase::admob::IsInitialized()) {
                                           firebase::admob::Terminate();
                                         }
                                       });
    }
  }
}

void UnregisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kAdMobModuleName)) {
    App* app = App::GetInstance();
    if (app) {
      CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(app);
      assert(cleanup_notifier);
      cleanup_notifier->UnregisterObject(const_cast<char*>(kAdMobModuleName));
    }
  }
}

CleanupNotifier* GetOrCreateCleanupNotifier() {
  if (!g_cleanup_notifier) {
    g_cleanup_notifier = new CleanupNotifier();
  }
  return g_cleanup_notifier;
}

void DestroyCleanupNotifier() {
  delete g_cleanup_notifier;
  g_cleanup_notifier = nullptr;
}

// Error messages used for completing futures. These match the error codes in
// the AdMobError enumeration in the C++ API.
const char* kAdUninitializedErrorMessage = "Ad has not been fully initialized.";
const char* kAdLoadInProgressErrorMessage = "Ad is currently loading.";
const char* kInternalSDKErrorMesage = "An internal SDK error occurred.";

const char* GetRequestAgentString() {
  // This is a string that can be used to uniquely identify requests coming
  // from this version of the library.
  return "firebase-cpp-api." FIREBASE_VERSION_NUMBER_STRING;
}

// Create a future and update the corresponding last result.
template <class T>
SafeFutureHandle<T> CreateFuture(int fn_idx, FutureData* future_data) {
  return future_data->future_impl.SafeAlloc<T>(fn_idx);
}

// Mark a future as complete.
void CompleteFuture(int error, const char* error_msg, SafeFutureHandle<void> handle,
                    FutureData* future_data) {
  future_data->future_impl.Complete(handle, error, error_msg);
}

template <class T>
void CompleteFuture(int error, const char* error_msg, SafeFutureHandle<T> handle,
                    FutureData* future_data, const T& result) {
  future_data->future_impl.Complete(handle, error, error_msg);
}

// For calls that aren't asynchronous, we can create and complete at the
// same time.
Future<void> CreateAndCompleteFuture(int fn_idx, int error, const char* error_msg,
                             FutureData* future_data) {
  SafeFutureHandle<void> handle = CreateFuture<void>(fn_idx, future_data);
  CompleteFuture(error, error_msg, handle, future_data);
  return MakeFuture(&future_data->future_impl, handle);
}

template <class T>
Future<T> CreateAndCompleteFutureWithResult(int fn_idx, int error, const char* error_msg, FutureData* future_data, const T& result) {
  //SafeFutureHandle<T> handle = future_data->future_impl.SafeAlloc<T>(fn_idx);
  SafeFutureHandle<T> handle = CreateFuture<T>(fn_idx, future_data);
  //future_data->future_impl.Complete(handle, error, error_msg, result);
  CompleteFuture(error, error_msg, handle, future_data, result);
  return MakeFuture(&future_data->future_impl, handle);
}

// Non-inline implementation of the Listeners' virtual destructors, to prevent
// their vtables from being emitted in each translation unit.
BannerView::Listener::~Listener() {}
InterstitialAd::Listener::~Listener() {}

}  // namespace admob
}  // namespace firebase
