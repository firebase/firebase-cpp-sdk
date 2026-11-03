// Copyright 2021 Google LLC

#include "performance/src/performance_common.h"

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
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

namespace firebase::performance::internal {

namespace {

Mutex& GetRegisteredAppMutex() {
  static Mutex* mutex = new Mutex();
  return *mutex;
}

App* g_registered_app = nullptr;

}  // namespace

// Matches extern const char* kPerformanceModuleName in performance_common.h.
const char* kPerformanceModuleName = "performance";

void RegisterTerminateOnDefaultAppDestroy(const App* app) {
  if (AppCallback::GetEnabledByName(kPerformanceModuleName)) {
    return;
  }
  App* target_app = app ? const_cast<App*>(app) : App::GetInstance();
  if (!target_app) {
    return;
  }
  CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(target_app);
  if (!cleanup_notifier) {
    return;
  }
  {
    MutexLock lock(GetRegisteredAppMutex());
    g_registered_app = target_app;
  }
  cleanup_notifier->RegisterObject(
      const_cast<char*>(kPerformanceModuleName),
      static_cast<CleanupNotifier::CleanupCallback>([](void*) {
        {
          MutexLock lock(GetRegisteredAppMutex());
          g_registered_app = nullptr;
        }
        LogError(
            "performance::Terminate() should be called before default app is "
            "destroyed.");
        if (firebase::performance::internal::IsInitialized()) {
          firebase::performance::Terminate();
        }
      }));
}

void UnregisterTerminateOnDefaultAppDestroy(const App* app) {
  App* target_app = nullptr;
  {
    MutexLock lock(GetRegisteredAppMutex());
    if (g_registered_app == nullptr) {
      return;
    }
    target_app = app ? const_cast<App*>(app) : g_registered_app;
    g_registered_app = nullptr;
  }
  CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(target_app);
  if (cleanup_notifier) {
    cleanup_notifier->UnregisterObject(
        const_cast<char*>(kPerformanceModuleName));
  }
}

}  // namespace firebase::performance::internal
