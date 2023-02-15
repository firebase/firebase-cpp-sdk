// Copyright 2021 Google LLC

#include "performance/src/performance_common.h"

#include <assert.h>

#include "app/src/cleanup_notifier.h"
#include "app/src/util.h"
#include "performance/src/include/firebase/performance.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    performance,
    {
      if (app == ::firebase::App::GetInstance()) {
        return firebase::performance::Initialize(*app);
      }
      return kInitResultSuccess;
    },
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::performance::Terminate();
      }
    },
    false);

namespace firebase {
namespace performance {
namespace internal {

const char* kPerformanceModuleName = "performance";

void RegisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kPerformanceModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(App::GetInstance());
    assert(cleanup_notifier);
    cleanup_notifier->RegisterObject(
        const_cast<char*>(kPerformanceModuleName), [](void*) {
          LogError(
              "performance::Terminate() should be called before default app is "
              "destroyed.");
          if (firebase::performance::internal::IsInitialized()) {
            firebase::performance::Terminate();
          }
        });
  }
}

void UnregisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kPerformanceModuleName) &&
      firebase::performance::internal::IsInitialized()) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(App::GetInstance());
    assert(cleanup_notifier);
    cleanup_notifier->UnregisterObject(
        const_cast<char*>(kPerformanceModuleName));
  }
}

}  // namespace internal
}  // namespace performance
}  // namespace firebase
