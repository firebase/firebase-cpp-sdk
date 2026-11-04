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

#include "app/src/app_android.h"
#include "app/src/include/firebase/util.h"

#include <assert.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include "app/src/google_play_services/availability_android.h"
#include "app/src/include/google_play_services/availability.h"
#endif  // FIREBASE_PLATFORM_ANDROID
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"

namespace firebase {

enum ModuleInitializerFn {
  kModuleInitializerInitialize,
  kModuleInitializerCount
};

struct ModuleInitializerState {
  ModuleInitializerState()
      : future_impl(kModuleInitializerCount),
        app(nullptr),
        context(nullptr),
        init_fn_idx(0),
        cancelled(false) {}
  Mutex mutex;
  // Futures implementation.
  ReferenceCountedFutureImpl future_impl;
  // Handle to the Initialize() future.
  SafeFutureHandle<void> future_handle_init;

  // Data we will pass to the user's callbacks.
  App* app;
  void* context;

  // Initialization callbacks. These will each be called in order, but if any
  // of them returns kInitResultFailedMissingDependency, we'll stop and update,
  // then resume where we left off.
  std::vector<ModuleInitializer::InitializerFn> init_fns;

  // Where we are in the initializer function list.
  size_t init_fn_idx;

  // True once ~ModuleInitializer() has run.
  bool cancelled;
};

struct ModuleInitializerData {
  ModuleInitializerData()
      : state(std::make_shared<ModuleInitializerState>()) {}
  std::shared_ptr<ModuleInitializerState> state;
};

ModuleInitializer::ModuleInitializer() { data_ = new ModuleInitializerData; }

ModuleInitializer::~ModuleInitializer() {
  if (data_ && data_->state) {
    MutexLock lock(data_->state->mutex);
    data_->state->cancelled = true;
    data_->state->app = nullptr;
    data_->state->context = nullptr;
  }
  delete data_;
  data_ = nullptr;
}

static void PerformInitialize(
    const std::shared_ptr<ModuleInitializerState>& state) {
  MutexLock lock(state->mutex);
  while (state->init_fn_idx < state->init_fns.size()) {
    if (state->cancelled) {
      return;
    }
    ModuleInitializer::InitializerFn fn = state->init_fns[state->init_fn_idx];
    if (!fn) {
      int num_remaining =
          static_cast<int>(state->init_fns.size() - state->init_fn_idx);
      state->future_impl.Complete(
          state->future_handle_init, num_remaining,
          "ModuleInitializer callback function pointer is null.");
      return;
    }
    InitResult init_result = fn(state->app, state->context);
    if (state->cancelled) {
      return;
    }

#if FIREBASE_PLATFORM_ANDROID
    if (init_result == kInitResultFailedMissingDependency) {
      // Note: If Initialize here succeeds, google_play_services::Terminate
      // is called in the OnCompletion handler below. Note that these
      // are reference-counted so it's safe to init/terminate an extra time.
      if (state->app != nullptr &&
          google_play_services::Initialize(state->app->GetJNIEnv(),
                                           state->app->activity())) {
        // On Android, we need to update or activate Google Play services
        // before we can initialize this Firebase module.
        LogWarning("Google Play services unavailable, trying to fix.");

        Future<void> make_available = google_play_services::MakeAvailable(
            state->app->GetJNIEnv(), state->app->activity());
        if (make_available.status() == kFutureStatusInvalid) {
          int num_remaining =
              static_cast<int>(state->init_fns.size() - state->init_fn_idx);
          state->future_impl.Complete(
              state->future_handle_init, num_remaining,
              "Unable to initialize due to missing Google Play services "
              "dependency.");
          google_play_services::Terminate(state->app->GetJNIEnv());
          return;
        }

        auto* state_holder = new std::shared_ptr<ModuleInitializerState>(state);
        make_available.OnCompletion(
            [](const Future<void>& result, void* ptr) {
              std::unique_ptr<std::shared_ptr<ModuleInitializerState>> holder(
                  reinterpret_cast<std::shared_ptr<ModuleInitializerState>*>(
                      ptr));
              std::shared_ptr<ModuleInitializerState> cb_state = *holder;
              MutexLock cb_lock(cb_state->mutex);
              if (cb_state->cancelled) {
                google_play_services::Terminate(firebase::util::GetJNIEnvFromApp());
                return;
              }
              JNIEnv* env = cb_state->app != nullptr
                                ? cb_state->app->GetJNIEnv()
                                : firebase::util::GetJNIEnvFromApp();
              if (result.status() == kFutureStatusComplete) {
                if (result.error() == 0) {
                  LogInfo("Google Play services now available, continuing.");
                  PerformInitialize(cb_state);
                  google_play_services::Terminate(env);
                } else {
                  LogError("Google Play services still unavailable.");
                  int num_remaining = static_cast<int>(
                      cb_state->init_fns.size() - cb_state->init_fn_idx);
                  cb_state->future_impl.Complete(
                      cb_state->future_handle_init, num_remaining,
                      "Unable to initialize due to "
                      "missing Google Play services "
                      "dependency.");
                  google_play_services::Terminate(env);
                }
              } else {
                int num_remaining = static_cast<int>(
                    cb_state->init_fns.size() - cb_state->init_fn_idx);
                cb_state->future_impl.Complete(
                    cb_state->future_handle_init, num_remaining,
                    "Google Play services availability check was invalidated.");
                google_play_services::Terminate(env);
              }
            },
            state_holder);
      } else {
        int num_remaining =
            static_cast<int>(state->init_fns.size() - state->init_fn_idx);
        state->future_impl.Complete(
            state->future_handle_init, num_remaining,
            "Could not run Google Play services update due to app "
            "misconfiguration. Please add "
            "com.google.android.gms:play-services-base as an Android "
            "dependency to enable this functionality.");
      }
      return;
    }
#else   // !FIREBASE_PLATFORM_ANDROID
    // Outside of Android, we shouldn't get kInitResultFailedMissingDependency.
    FIREBASE_ASSERT(init_result != kInitResultFailedMissingDependency);
#endif  // FIREBASE_PLATFORM_ANDROID
    if (init_result == kInitResultSuccess) {
      state->init_fn_idx++;  // This function succeeded, move on to the next.
    } else {
      int num_remaining =
          static_cast<int>(state->init_fns.size() - state->init_fn_idx);
      state->future_impl.Complete(state->future_handle_init, num_remaining,
                                  "Module initialization failed.");
      return;
    }
  }
  state->future_impl.Complete(state->future_handle_init, 0);
}

Future<void> ModuleInitializer::Initialize(
    App* app, void* context, ModuleInitializer::InitializerFn init_fn) {
  FIREBASE_ASSERT_RETURN(Future<void>(), app != nullptr);
  FIREBASE_ASSERT_RETURN(Future<void>(), init_fn != nullptr);

  return Initialize(app, context, &init_fn, 1);
}

Future<void> ModuleInitializer::Initialize(
    App* app, void* context, const ModuleInitializer::InitializerFn* init_fns,
    size_t init_fns_count) {
  FIREBASE_ASSERT_RETURN(Future<void>(), app != nullptr);
  FIREBASE_ASSERT_RETURN(Future<void>(),
                         init_fns != nullptr || init_fns_count == 0);

  std::shared_ptr<ModuleInitializerState>& state = data_->state;
  MutexLock lock(state->mutex);
  if (InitializeLastResult().status() != kFutureStatusPending) {
    state->future_handle_init =
        state->future_impl.SafeAlloc<void>(kModuleInitializerInitialize);
    state->app = app;
    state->init_fn_idx = 0;
    state->init_fns.clear();
    for (size_t i = 0; i < init_fns_count; i++) {
      state->init_fns.push_back(init_fns[i]);
    }
    state->context = context;
    PerformInitialize(state);
  }
  return InitializeLastResult();
}

Future<void> ModuleInitializer::InitializeLastResult() {
  return static_cast<const Future<void>&>(
      data_->state->future_impl.LastResult(kModuleInitializerInitialize));
}

Future<void> ModuleInitializer::InitializeLastResult() const {
  return static_cast<const Future<void>&>(
      data_->state->future_impl.LastResult(kModuleInitializerInitialize));
}

std::map<std::string, AppCallback*>* AppCallback::callbacks_ = nullptr;

static Mutex* GetCallbacksMutex() {
  static Mutex* s_mutex = new Mutex();
  return s_mutex;
}

void AppCallback::NotifyAllAppCreated(
    firebase::App* app, std::map<std::string, InitResult>* results) {
  if (results) results->clear();
  MutexLock lock(*GetCallbacksMutex());
  if (!callbacks_) return;
  for (std::map<std::string, AppCallback*>::const_iterator it =
           callbacks_->begin();
       it != callbacks_->end(); ++it) {
    const AppCallback* callback = it->second;
    if (callback && callback->enabled()) {
      InitResult result = callback->NotifyAppCreated(app);
      if (results) (*results)[it->first] = result;
    }
  }
}

void AppCallback::NotifyAllAppDestroyed(firebase::App* app) {
  MutexLock lock(*GetCallbacksMutex());
  if (!callbacks_) return;
  for (std::map<std::string, AppCallback*>::const_iterator it =
           callbacks_->begin();
       it != callbacks_->end(); ++it) {
    const AppCallback* callback = it->second;
    if (callback && callback->enabled()) callback->NotifyAppDestroyed(app);
  }
}

// Determine whether a module callback is enabled, by name.
bool AppCallback::GetEnabledByName(const char* name) {
  if (!name) return false;
  MutexLock lock(*GetCallbacksMutex());
  if (!callbacks_) return false;
  std::map<std::string, AppCallback*>::const_iterator it =
      callbacks_->find(std::string(name));
  if (it == callbacks_->end() || !it->second) {
    return false;
  }
  return it->second->enabled();
}

// Enable a module callback by name.
void AppCallback::SetEnabledByName(const char* name, bool enable) {
  if (!name) return;
  MutexLock lock(*GetCallbacksMutex());
  if (!callbacks_) return;
  std::map<std::string, AppCallback*>::const_iterator it =
      callbacks_->find(std::string(name));
  if (it == callbacks_->end() || !it->second) {
    LogDebug("App initializer %s not found, failed to enable.", name);
    return;
  }
  LogDebug("%s app initializer %s", enable ? "Enabling" : "Disabling", name);
  it->second->set_enabled(enable);
}

void AppCallback::SetEnabledAll(bool enable) {
  MutexLock lock(*GetCallbacksMutex());
  if (!callbacks_) return;
  LogDebug("%s all app initializers", enable ? "Enabling" : "Disabling");
  for (std::map<std::string, AppCallback*>::const_iterator it =
           callbacks_->begin();
       it != callbacks_->end(); ++it) {
    if (!it->second) continue;
    LogDebug("%s %s", enable ? "Enable" : "Disable", it->second->module_name());
    it->second->set_enabled(enable);
  }
}

void AppCallback::AddCallback(AppCallback* callback) {
  if (!callback || !callback->module_name()) return;
  MutexLock lock(*GetCallbacksMutex());
  if (!callbacks_) {
    callbacks_ = new std::map<std::string, AppCallback*>();
  }
  std::string name = callback->module_name();
  if (callbacks_->find(name) != callbacks_->end()) {
    LogWarning(
        "%s is already registered for callbacks on app initialization,  "
        "ignoring.",
        name.c_str());
  } else {
    LogDebug("Registered app initializer %s (enabled: %d)", name.c_str(),
              callback->enabled() ? 1 : 0);
    (*callbacks_)[name] = callback;
  }
}

static Mutex* GetFuturesMutex() {
  static Mutex* s_futures_mutex = new Mutex();
  return s_futures_mutex;
}

static std::map<const void*, std::unique_ptr<StaticFutureData>>*
GetFutureDatasMap() {
  static auto* s_future_datas =
      new std::map<const void*, std::unique_ptr<StaticFutureData>>();
  return s_future_datas;
}

// static
void StaticFutureData::CleanupFutureDataForModule(
    const void* module_identifier) {
  std::unique_ptr<StaticFutureData> data_to_delete;
  {
    MutexLock lock(*GetFuturesMutex());
    auto* future_datas = GetFutureDatasMap();
    auto it = future_datas->find(module_identifier);
    if (it != future_datas->end()) {
      data_to_delete = std::move(it->second);
      future_datas->erase(it);
    }
  }
}

// static
StaticFutureData* StaticFutureData::GetFutureDataForModule(
    const void* module_identifier, int num_functions) {
  MutexLock lock(*GetFuturesMutex());
  auto* future_datas = GetFutureDatasMap();
  auto& data = (*future_datas)[module_identifier];
  if (!data) {
    data.reset(CreateNewData(module_identifier, num_functions));
  }
  return data.get();
}

// static
StaticFutureData* StaticFutureData::CreateNewData(const void* module_identifier,
                                                  int num_functions) {
  (void)module_identifier;
  StaticFutureData* new_data = new StaticFutureData(num_functions);
  return new_data;
}

std::vector<std::string> SplitString(const std::string& s,
                                     const char delimiter) {
  std::vector<std::string> split_parts;
  size_t len = s.size();
  size_t delimiter_search_start = 0;
  // Skip any leading delimiters
  while (delimiter_search_start < len &&
         s[delimiter_search_start] == delimiter) {
    delimiter_search_start++;
  }

  // Can't proceed if input string consists of just delimiters
  if (delimiter_search_start >= len) {
    return split_parts;
  }

  size_t pos = 0;
  while ((pos = s.find(delimiter, delimiter_search_start)) !=
         std::string::npos) {
    split_parts.push_back(
        s.substr(delimiter_search_start, pos - delimiter_search_start));

    while (pos < len && s[pos] == delimiter) {
      pos++;
    }
    delimiter_search_start = pos;
  }

  // If the input string doesn't end with a delimiter we need to push the last
  // token into our return vector
  if (delimiter_search_start < len) {
    split_parts.push_back(
        s.substr(delimiter_search_start, len - delimiter_search_start));
  }
  return split_parts;
}

// Id used as part of the logic to make unique api identifiers.
static std::atomic<int> g_api_identifier_count{0};

std::string CreateApiIdentifier(const char* api_id, const void* object) {
  const char* safe_api_id = api_id ? api_id : "";
  const char* format = "%s0x%016llx_%d";
  unsigned long long object_ptr =       // NOLINT
      static_cast<unsigned long long>(  // NOLINT
          reinterpret_cast<uintptr_t>(object));
  int extra_id = g_api_identifier_count.fetch_add(1, std::memory_order_relaxed);
  int length = snprintf(nullptr, 0, format, safe_api_id, object_ptr, extra_id);
  if (length <= 0) return std::string();
  std::string created(static_cast<size_t>(length), '\0');
  snprintf(&created[0], static_cast<size_t>(length) + 1, format, safe_api_id,
           object_ptr, extra_id);
  return created;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
