#include "analytics/src/analytics_common.h"
#include "analytics/src/include/firebase/analytics.h"
#include "analytics/src/include/firebase/analytics/event_names.h"
#include "analytics/src/include/firebase/analytics/parameter_names.h"
#include "analytics/src/include/firebase/analytics/user_property_names.h"
#include "app/src/app_common.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/util.h"
FIREBASE_APP_REGISTER_CALLBACKS(
    analytics,
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::analytics::Initialize(*app);
      }
      return kInitResultSuccess;
    },
    {
      if (app == ::firebase::App::GetInstance() &&
          firebase::analytics::internal::IsInitialized()) {
        firebase::analytics::Terminate();
      }
    },
    false);
namespace firebase {
namespace analytics {
namespace internal {
const char* const kAnalyticsModuleName = "analytics";
static App* s_registered_app = nullptr;
static std::recursive_mutex& RegisteredAppMutex() {
  static std::recursive_mutex s_mutex;
  return s_mutex;
}
std::shared_ptr<FutureData>& FutureData::FutureDataPtr() {
  static std::shared_ptr<FutureData> s_future_data;
  return s_future_data;
}
std::recursive_mutex& FutureData::mutex() {
  static std::recursive_mutex s_mutex;
  return s_mutex;
}
FutureData* FutureData::Create() {
  std::lock_guard<std::recursive_mutex> lock(mutex());
  auto& future_data = FutureDataPtr();
  future_data = std::make_shared<FutureData>();
  return future_data.get();
}
void FutureData::Destroy() {
  std::lock_guard<std::recursive_mutex> lock(mutex());
  FutureDataPtr().reset();
}
FutureData* FutureData::Get() {
  std::lock_guard<std::recursive_mutex> lock(mutex());
  return FutureDataPtr().get();
}
std::shared_ptr<FutureData> FutureData::GetShared() {
  std::lock_guard<std::recursive_mutex> lock(mutex());
  return FutureDataPtr();
}
void RegisterTerminateOnAppDestroy(const App* app) {
  App* target_app = const_cast<App*>(app ? app : App::GetInstance());
  CleanupNotifier* old_notifier = nullptr;
  CleanupNotifier* target_notifier = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(RegisteredAppMutex());
    if (s_registered_app && s_registered_app != target_app) {
      old_notifier = CleanupNotifier::FindByOwner(s_registered_app);
      s_registered_app = nullptr;
    }
    if (target_app &&
        (target_notifier = CleanupNotifier::FindByOwner(target_app))) {
      s_registered_app = target_app;
    }
  }
  if (old_notifier) {
    old_notifier->UnregisterObject(const_cast<char*>(kAnalyticsModuleName));
  }
  if (target_notifier) {
    target_notifier->RegisterObject(
        const_cast<char*>(kAnalyticsModuleName), [](void*) {
          {
            std::lock_guard<std::recursive_mutex> lock(RegisteredAppMutex());
            s_registered_app = nullptr;
          }
          if (firebase::analytics::internal::IsInitialized()) {
            firebase::analytics::Terminate();
          }
        });
  }
}
void RegisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kAnalyticsModuleName)) {
    RegisterTerminateOnAppDestroy(App::GetInstance());
  }
}
void UnregisterTerminateOnAppDestroy() {
  CleanupNotifier* cleanup_notifier = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(RegisteredAppMutex());
    if (s_registered_app) {
      cleanup_notifier = CleanupNotifier::FindByOwner(s_registered_app);
      s_registered_app = nullptr;
    }
  }
  if (cleanup_notifier) {
    cleanup_notifier->UnregisterObject(const_cast<char*>(kAnalyticsModuleName));
  }
}
void UnregisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kAnalyticsModuleName)) {
    UnregisterTerminateOnAppDestroy();
  }
}
}  // namespace internal
}  // namespace analytics
}  // namespace firebase
