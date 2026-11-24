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

#include "app/src/include/firebase/util.h"

#include <assert.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include "app/src/google_play_services/availability_android.h"
#include "app/src/include/google_play_services/availability.h"
#include "app/src/util_android.h"
#endif  // FIREBASE_PLATFORM_ANDROID
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/no_destructor.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"

namespace firebase {

enum ModuleInitializerFn {
  kModuleInitializerInitialize,
  kModuleInitializerCount
};

struct ModuleInitializerData {
  ModuleInitializerData()
      : future_impl(kModuleInitializerCount),
        context(nullptr),
        init_fn_idx(0) {}
  ReferenceCountedFutureImpl future_impl;
  SafeFutureHandle<void> future_handle_init;

  // Value handle keeps AppState alive across asynchronous GPS resolution.
  App app;
  void* context;

  std::vector<ModuleInitializer::InitializerFn> init_fns;
  size_t init_fn_idx;
};

ModuleInitializer::ModuleInitializer() { data_ = new ModuleInitializerData; }

ModuleInitializer::~ModuleInitializer() {
  delete data_;
  data_ = nullptr;
}

static void PerformInitialize(ModuleInitializerData* data) {
  while (data->init_fn_idx < data->init_fns.size()) {
    InitResult init_result =
        data->init_fns[data->init_fn_idx](data->app, data->context);

#if FIREBASE_PLATFORM_ANDROID
    if (init_result == kInitResultFailedMissingDependency) {
      if (google_play_services::Initialize(data->app.GetJNIEnv(),
                                           data->app.activity())) {
        LogWarning("Google Play services unavailable, trying to fix.");

        Future<void> make_available = google_play_services::MakeAvailable(
            data->app.GetJNIEnv(), data->app.activity());

        make_available.OnCompletion(
            [](const Future<void>& result, void* ptr) {
              ModuleInitializerData* data =
                  reinterpret_cast<ModuleInitializerData*>(ptr);
              if (result.status() == kFutureStatusComplete) {
                if (result.error() == 0) {
                  LogInfo("Google Play services now available, continuing.");
                  PerformInitialize(data);
                  google_play_services::Terminate(data->app.GetJNIEnv());
                } else {
                  LogError("Google Play services still unavailable.");
                  int num_remaining = static_cast<int>(data->init_fns.size() -
                                                       data->init_fn_idx);
                  data->future_impl.Complete(data->future_handle_init,
                                             num_remaining,
                                             "Unable to initialize due to "
                                             "missing Google Play services "
                                             "dependency.");
                  google_play_services::Terminate(util::GetJNIEnvFromApp());
                }
              }
            },
            data);
      } else {
        int num_remaining =
            static_cast<int>(data->init_fns.size() - data->init_fn_idx);
        data->future_impl.Complete(
            data->future_handle_init, num_remaining,
            "Could not run Google Play services update due to app "
            "misconfiguration. Please add "
            "com.google.android.gms:play-services-base as an Android "
            "dependency to enable this functionality.");
      }
    }
#else   // !FIREBASE_PLATFORM_ANDROID
    FIREBASE_ASSERT(init_result != kInitResultFailedMissingDependency);
#endif  // FIREBASE_PLATFORM_ANDROID
    if (init_result == kInitResultSuccess) {
      data->init_fn_idx++;
    } else {
      return;
    }
  }
  data->future_impl.Complete(data->future_handle_init, 0);
}

Future<void> ModuleInitializer::Initialize(
    const App& app, void* context, ModuleInitializer::InitializerFn init_fn) {
  FIREBASE_ASSERT(app.is_valid());
  FIREBASE_ASSERT(init_fn != nullptr);

  return Initialize(app, context, &init_fn, 1);
}

Future<void> ModuleInitializer::Initialize(
    const App& app, void* context,
    const ModuleInitializer::InitializerFn* init_fns, size_t init_fns_count) {
  FIREBASE_ASSERT(app.is_valid());
  FIREBASE_ASSERT(init_fns != nullptr);

  if (!data_->future_impl.ValidFuture(data_->future_handle_init)) {
    data_->future_handle_init =
        data_->future_impl.SafeAlloc<void>(kModuleInitializerInitialize);
    data_->app = app;
    data_->init_fn_idx = 0;
    data_->init_fns.clear();
    for (size_t i = 0; i < init_fns_count; i++) {
      data_->init_fns.push_back(init_fns[i]);
    }
    data_->context = context;
    PerformInitialize(data_);
  }
  return InitializeLastResult();
}

Future<void> ModuleInitializer::InitializeLastResult() {
  return static_cast<const Future<void>&>(
      data_->future_impl.LastResult(kModuleInitializerInitialize));
}

namespace {

struct AppCallbackRegistry {
  Mutex mutex;
  std::map<std::string, AppCallback*> callbacks;
};

AppCallbackRegistry& GetAppCallbackRegistry() {
  static internal::NoDestructor<AppCallbackRegistry> registry;
  return *registry;
}

struct StaticFutureRegistry {
  Mutex mutex;
  std::map<const void*, StaticFutureData*> datas;
};

StaticFutureRegistry& GetStaticFutureRegistry() {
  static internal::NoDestructor<StaticFutureRegistry> registry;
  return *registry;
}

}  // namespace

void AppCallback::NotifyAllAppCreated(
    const firebase::App& app, std::map<std::string, InitResult>* results) {
  if (results) results->clear();
  AppCallbackRegistry& registry = GetAppCallbackRegistry();
  MutexLock lock(registry.mutex);
  for (const auto& entry : registry.callbacks) {
    const AppCallback* callback = entry.second;
    if (callback->enabled()) {
      InitResult result = callback->NotifyAppCreated(app);
      if (results) (*results)[entry.first] = result;
    }
  }
}

bool AppCallback::GetEnabledByName(const char* name) {
  AppCallbackRegistry& registry = GetAppCallbackRegistry();
  MutexLock lock(registry.mutex);
  auto it = registry.callbacks.find(std::string(name));
  if (it == registry.callbacks.end()) {
    return false;
  }
  return it->second->enabled();
}

void AppCallback::SetEnabledByName(const char* name, bool enable) {
  AppCallbackRegistry& registry = GetAppCallbackRegistry();
  MutexLock lock(registry.mutex);
  auto it = registry.callbacks.find(std::string(name));
  if (it == registry.callbacks.end()) {
    LogDebug("App initializer %s not found, failed to enable.", name);
    return;
  }
  LogDebug("%s app initializer %s", name, enable ? "Enabling" : "Disabling");
  it->second->set_enabled(enable);
}

void AppCallback::SetEnabledAll(bool enable) {
  AppCallbackRegistry& registry = GetAppCallbackRegistry();
  MutexLock lock(registry.mutex);
  LogDebug("%s all app initializers", enable ? "Enabling" : "Disabling");
  for (auto& entry : registry.callbacks) {
    LogDebug("%s %s", enable ? "Enable" : "Disable",
             entry.second->module_name());
    entry.second->set_enabled(enable);
  }
}

void AppCallback::AddCallback(AppCallback* callback) {
  AppCallbackRegistry& registry = GetAppCallbackRegistry();
  std::string name = callback->module_name();
  if (registry.callbacks.find(name) != registry.callbacks.end()) {
    LogWarning(
        "%s is already registered for callbacks on app initialization, "
        "ignoring.",
        name.c_str());
  } else {
    LogDebug("Registered app initializer %s (enabled: %d)", name.c_str(),
             callback->enabled() ? 1 : 0);
    registry.callbacks[name] = callback;
  }
}

// static
void StaticFutureData::CleanupFutureDataForModule(
    const void* module_identifier) {
  StaticFutureRegistry& registry = GetStaticFutureRegistry();
  MutexLock lock(registry.mutex);

  auto it = registry.datas.find(module_identifier);
  if (it != registry.datas.end()) {
    delete it->second;
    registry.datas.erase(it);
  }
}

// static
StaticFutureData* StaticFutureData::GetFutureDataForModule(
    const void* module_identifier, int num_functions) {
  StaticFutureRegistry& registry = GetStaticFutureRegistry();
  MutexLock lock(registry.mutex);

  auto it = registry.datas.find(module_identifier);
  if (it != registry.datas.end() && it->second != nullptr) {
    return it->second;
  }

  StaticFutureData* new_data = CreateNewData(module_identifier, num_functions);
  registry.datas[module_identifier] = new_data;
  return new_data;
}

// static
StaticFutureData* StaticFutureData::CreateNewData(const void* /*module_id*/,
                                                  int num_functions) {
  return new StaticFutureData(num_functions);
}

std::vector<std::string> SplitString(const std::string& s,
                                     const char delimiter) {
  size_t pos = 0;
  size_t delimiter_search_start = 0;
  size_t len = s.size();
  while (delimiter_search_start < len &&
         s[delimiter_search_start] == delimiter) {
    delimiter_search_start++;
  }

  std::vector<std::string> split_parts;
  if (delimiter_search_start >= len) {
    return split_parts;
  }

  while ((pos = s.find(delimiter, delimiter_search_start)) !=
         std::string::npos) {
    split_parts.push_back(
        s.substr(delimiter_search_start, pos - delimiter_search_start));

    while (pos < len && s[pos] == delimiter) {
      pos++;
      delimiter_search_start = pos;
    }
  }

  if (delimiter_search_start != len) {
    split_parts.push_back(
        s.substr(delimiter_search_start, len - delimiter_search_start));
  }
  return split_parts;
}

static std::atomic<int> g_api_identifier_count{0};

std::string CreateApiIdentifier(const char* api_id, void* object) {
  const char* format = "%s0x%016llx_%d";
  unsigned long long object_ptr =       // NOLINT
      static_cast<unsigned long long>(  // NOLINT
          reinterpret_cast<intptr_t>(object));
  int extra_id = g_api_identifier_count.fetch_add(1, std::memory_order_relaxed);
  int size = snprintf(nullptr, 0, format, api_id, object_ptr, extra_id);
  std::string created(static_cast<size_t>(size), '\0');
  snprintf(&created[0], created.size() + 1, format, api_id, object_ptr,
           extra_id);
  return created;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
