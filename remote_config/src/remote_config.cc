#include "remote_config/src/include/firebase/remote_config.h"
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/time.h"
#include "app/src/util.h"
#include "remote_config/src/common.h"
#if FIREBASE_PLATFORM_ANDROID
#include "remote_config/src/android/remote_config_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "remote_config/src/ios/remote_config_ios.h"
#else
#include "remote_config/src/desktop/remote_config_desktop.h"
#endif
namespace firebase::remote_config {
DEFINE_FIREBASE_VERSION_STRING(FirebaseRemoteConfig);
namespace {
Mutex& GetRcLock() {
  static auto* lock = new Mutex();
  return *lock;
}
std::map<App*, RemoteConfig*>& GetRcInstances() {
  static auto* instances = new std::map<App*, RemoteConfig*>();
  return *instances;
}
RemoteConfig* FindRemoteConfigLocked(App* app) {
  auto& instances = GetRcInstances();
  auto it = instances.find(app);
  return it != instances.end() ? it->second : nullptr;
}
void SetStaticValueInfo(ValueInfo* info) {
  if (info) {
    info->source = kValueSourceStaticValue;
    info->conversion_successful = false;
  }
}
}  // namespace
RemoteConfig* RemoteConfig::GetInstance(App* app) {
  if (!app) {
    LogError("RemoteConfig::GetInstance: App cannot be null.");
    return nullptr;
  }
  RemoteConfig* failed_rc = nullptr;
  {
    MutexLock lock(GetRcLock());
    if (RemoteConfig* existing_rc = FindRemoteConfigLocked(app)) {
      return existing_rc;
    }
    auto* rc = new RemoteConfig(app);
    if (!rc->InitInternal()) {
      LogError("Failed to initialize RemoteConfig instance for App %s",
               app->name());
      failed_rc = rc;
    } else {
      CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
      FIREBASE_ASSERT(notifier);
      notifier->RegisterObject(rc, [](void* object) {
        auto* remote_config = reinterpret_cast<RemoteConfig*>(object);
        LogWarning(
            "RemoteConfig object %p should be deleted before the App %p it "
            "depends upon.",
            remote_config, remote_config->app());
        remote_config->Cleanup();
      });
      GetRcInstances()[app] = rc;
      return rc;
    }
  }
  delete failed_rc;
  return nullptr;
}
RemoteConfig* RemoteConfig::FindRemoteConfig(App* app) {
  MutexLock lock(GetRcLock());
  return FindRemoteConfigLocked(app);
}
void RemoteConfig::Cleanup() {
  internal::RemoteConfigInternal* internal_to_delete = nullptr;
  {
    MutexLock lock(GetRcLock());
    internal_to_delete = std::exchange(internal_, nullptr);
    if (internal_to_delete && app_) {
      auto& instances = GetRcInstances();
      if (auto it = instances.find(app_);
          it != instances.end() && it->second == this) {
        instances.erase(it);
      }
    }
  }
  if (app_) {
    if (CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_)) {
      notifier->UnregisterObject(this);
    }
  }
  if (internal_to_delete) {
    internal_to_delete->Cleanup();
    delete internal_to_delete;
  }
}
RemoteConfig::RemoteConfig(App* app) : app_(app), internal_(nullptr) {}
bool RemoteConfig::InitInternal() {
  if (!app_) return false;
  if (!internal_) internal_ = new internal::RemoteConfigInternal(*app_);
  return internal_->Initialized();
}
RemoteConfig::~RemoteConfig() { Cleanup(); }
#define RC_DELEGATE_FUTURE(Method, Type)                                 \
  Future<Type> RemoteConfig::Method() {                                  \
    return internal_ ? internal_->Method() : Future<Type>();             \
  }                                                                      \
  Future<Type> RemoteConfig::Method##LastResult() {                      \
    return std::as_const(*this).Method##LastResult();                    \
  }                                                                      \
  Future<Type> RemoteConfig::Method##LastResult() const {                \
    return internal_ ? internal_->Method##LastResult() : Future<Type>(); \
  }
RC_DELEGATE_FUTURE(EnsureInitialized, ConfigInfo)
RC_DELEGATE_FUTURE(Activate, bool)
RC_DELEGATE_FUTURE(FetchAndActivate, bool)
Future<void> RemoteConfig::Fetch() {
  if (!internal_) return Future<void>();
  return Fetch(GetConfigSettings().minimum_fetch_interval_in_milliseconds /
               firebase::internal::kMillisecondsPerSecond);
}
Future<void> RemoteConfig::Fetch(uint64_t cache_expiration_in_seconds) {
  return internal_ ? internal_->Fetch(cache_expiration_in_seconds)
                   : Future<void>();
}
Future<void> RemoteConfig::FetchLastResult() {
  return std::as_const(*this).FetchLastResult();
}
Future<void> RemoteConfig::FetchLastResult() const {
  return internal_ ? internal_->FetchLastResult() : Future<void>();
}
#if defined(__ANDROID__)
Future<void> RemoteConfig::SetDefaults(int defaults_resource_id) {
  return internal_ ? internal_->SetDefaults(defaults_resource_id)
                   : Future<void>();
}
#endif
Future<void> RemoteConfig::SetDefaults(const ConfigKeyValueVariant* defaults,
                                       size_t number_of_defaults) {
  return internal_ ? internal_->SetDefaults(defaults, number_of_defaults)
                   : Future<void>();
}
Future<void> RemoteConfig::SetDefaults(const ConfigKeyValue* defaults,
                                       size_t number_of_defaults) {
  return internal_ ? internal_->SetDefaults(defaults, number_of_defaults)
                   : Future<void>();
}
Future<void> RemoteConfig::SetDefaultsLastResult() {
  return std::as_const(*this).SetDefaultsLastResult();
}
Future<void> RemoteConfig::SetDefaultsLastResult() const {
  return internal_ ? internal_->SetDefaultsLastResult() : Future<void>();
}
Future<void> RemoteConfig::SetConfigSettings(ConfigSettings settings) {
  return internal_ ? internal_->SetConfigSettings(settings) : Future<void>();
}
ConfigSettings RemoteConfig::GetConfigSettings() {
  return std::as_const(*this).GetConfigSettings();
}
ConfigSettings RemoteConfig::GetConfigSettings() const {
  return internal_ ? internal_->GetConfigSettings() : ConfigSettings{};
}
Future<void> RemoteConfig::SetConfigSettingsLastResult() {
  return std::as_const(*this).SetConfigSettingsLastResult();
}
Future<void> RemoteConfig::SetConfigSettingsLastResult() const {
  return internal_ ? internal_->SetConfigSettingsLastResult() : Future<void>();
}
#define RC_DELEGATE_GETTER(Method, Type, DefaultVal)                  \
  Type RemoteConfig::Method(const char* key) {                        \
    return std::as_const(*this).Method(key);                          \
  }                                                                   \
  Type RemoteConfig::Method(const char* key) const {                  \
    return Method(key, static_cast<ValueInfo*>(nullptr));             \
  }                                                                   \
  Type RemoteConfig::Method(const char* key, ValueInfo* info) {       \
    return std::as_const(*this).Method(key, info);                    \
  }                                                                   \
  Type RemoteConfig::Method(const char* key, ValueInfo* info) const { \
    if (!internal_) {                                                 \
      SetStaticValueInfo(info);                                       \
      return DefaultVal;                                              \
    }                                                                 \
    return internal_->Method(key, info);                              \
  }
RC_DELEGATE_GETTER(GetBoolean, bool, false)
RC_DELEGATE_GETTER(GetLong, int64_t, 0)
RC_DELEGATE_GETTER(GetDouble, double, 0.0)
RC_DELEGATE_GETTER(GetString, std::string, std::string{})
RC_DELEGATE_GETTER(GetData, std::vector<unsigned char>, std::vector<unsigned char>{})
std::vector<std::string> RemoteConfig::GetKeysByPrefix(const char* prefix) {
  return std::as_const(*this).GetKeysByPrefix(prefix);
}
std::vector<std::string> RemoteConfig::GetKeysByPrefix(
    const char* prefix) const {
  return internal_ ? internal_->GetKeysByPrefix(prefix)
                   : std::vector<std::string>{};
}
std::vector<std::string> RemoteConfig::GetKeys() {
  return std::as_const(*this).GetKeys();
}
std::vector<std::string> RemoteConfig::GetKeys() const {
  return internal_ ? internal_->GetKeys() : std::vector<std::string>{};
}
std::map<std::string, Variant> RemoteConfig::GetAll() {
  return std::as_const(*this).GetAll();
}
std::map<std::string, Variant> RemoteConfig::GetAll() const {
  return internal_ ? internal_->GetAll() : std::map<std::string, Variant>{};
}
const ConfigInfo RemoteConfig::GetInfo() {
  return std::as_const(*this).GetInfo();
}
const ConfigInfo RemoteConfig::GetInfo() const {
  return internal_ ? internal_->GetInfo()
                   : ConfigInfo{0, kLastFetchStatusPending,
                                kFetchFailureReasonInvalid, 0};
}
Future<void> RemoteConfig::SetCustomSignals(
    const std::map<std::string, Variant>& custom_signals) {
  return internal_ ? internal_->SetCustomSignals(custom_signals)
                   : Future<void>();
}
Future<void> RemoteConfig::SetCustomSignalsLastResult() {
  return std::as_const(*this).SetCustomSignalsLastResult();
}
Future<void> RemoteConfig::SetCustomSignalsLastResult() const {
  return internal_ ? internal_->SetCustomSignalsLastResult() : Future<void>();
}
ConfigUpdateListenerRegistration RemoteConfig::AddOnConfigUpdateListener(
    std::function<void(ConfigUpdate&&, RemoteConfigError)>
        config_update_listener) {
  return internal_ ? internal_->AddOnConfigUpdateListener(
                         std::move(config_update_listener))
                   : ConfigUpdateListenerRegistration();
}
}  // namespace firebase::remote_config
