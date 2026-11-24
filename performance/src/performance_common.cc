// Copyright 2021 Google LLC

#include "performance/src/performance_common.h"

#include <memory>
#include <string_view>

#include "app/src/app_state.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/no_destructor.h"
#include "app/src/util.h"
#include "performance/src/include/firebase/performance.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    performance,
    {
      if (std::string_view(app.name()) == kDefaultAppName) {
        return firebase::performance::Initialize(app);
      }
      return kInitResultSuccess;
    },
    false);

namespace firebase {
namespace performance {
namespace internal {

namespace {

constexpr int kPerformanceTeardownTag = 0;

struct TeardownGuardState {
  Mutex mutex;
  std::weak_ptr<::firebase::internal::AppState> app_state;
  std::weak_ptr<::firebase::internal::TeardownGuardService> guard;
};

TeardownGuardState& GetTeardownGuardState() {
  static ::firebase::internal::NoDestructor<TeardownGuardState> state;
  return *state;
}

}  // namespace

const char* kPerformanceModuleName = "performance";

void RegisterTerminateOnAppDestroy(const App& app) {
  const auto& app_state = ::firebase::internal::AppState::FromApp(app);
  if (!app_state) return;

  UnregisterTerminateOnAppDestroy();

  const bool auto_init = AppCallback::GetEnabledByName(kPerformanceModuleName);
  auto [owner, guard] =
      app_state->GetOrCreateDetachableService<
          ::firebase::internal::TeardownGuardService>(
          &kPerformanceTeardownTag, {},
          [auto_init](::firebase::internal::AppState&) {
            return std::make_shared<
                ::firebase::internal::TeardownGuardService>([auto_init]() {
              if (!auto_init) {
                LogError(
                    "performance::Terminate() should be called before default "
                    "app is destroyed.");
              }
              if (firebase::performance::internal::IsInitialized()) {
                firebase::performance::Terminate();
              }
            });
          });
  if (!guard) return;

  auto& state = GetTeardownGuardState();
  MutexLock lock(state.mutex);
  state.app_state = owner;
  state.guard = guard;
}

void UnregisterTerminateOnAppDestroy() {
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  std::weak_ptr<::firebase::internal::TeardownGuardService> weak_guard;
  {
    auto& state = GetTeardownGuardState();
    MutexLock lock(state.mutex);
    weak_state = std::move(state.app_state);
    weak_guard = std::move(state.guard);
  }
  auto guard = weak_guard.lock();
  if (guard) {
    guard->Cancel();
  }
  if (auto app_state = weak_state.lock()) {
    app_state->DetachService(&kPerformanceTeardownTag, {}, guard.get());
  }
}

}  // namespace internal
}  // namespace performance
}  // namespace firebase
