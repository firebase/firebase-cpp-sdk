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

#include <map>
#include <string>
#include <vector>

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include "app/src/include/google_play_services/availability.h"
#endif  // FIREBASE_PLATFORM_ANDROID
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

enum ModuleInitializerFn {
  kModuleInitializerInitialize,
  kModuleInitializerCount
};

struct ModuleInitializerData {
  ModuleInitializerData()
      : future_impl(kModuleInitializerCount),
        app(nullptr),
        context(nullptr),
        init_fn_idx(0) {}
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
  int init_fn_idx;
};

ModuleInitializer::ModuleInitializer() { data_ = new ModuleInitializerData; }

ModuleInitializer::~ModuleInitializer() {
  delete data_;
  data_ = nullptr;
}

static void PerformInitialize(ModuleInitializerData* data) {
  while (data->init_fn_idx < data->init_fns.size()) {
    InitResult init_result;
    init_result = data->init_fns[data->init_fn_idx](data->app, data->context);

#if FIREBASE_PLATFORM_ANDROID
    if (init_result == kInitResultFailedMissingDependency) {
      // On Android, we need to update or activate Google Play services
      // before we can initialize this Firebase module.
      LogWarning("Google Play services unavailable, trying to fix.");

      Future<void> make_available = google_play_services::MakeAvailable(
          data->app->GetJNIEnv(), data->app->activity());

      make_available.OnCompletion(
          [](const Future<void>& result, void* ptr) {
            ModuleInitializerData* data =
                reinterpret_cast<ModuleInitializerData*>(ptr);
            if (result.status() == kFutureStatusComplete) {
              if (result.error() == 0) {
                LogInfo("Google Play services now available, continuing.");
                PerformInitialize(data);
              } else {
                LogError("Google Play services still unavailable.");
                int num_remaining = data->init_fns.size() - data->init_fn_idx;
                data->future_impl.Complete(
                    data->future_handle_init, num_remaining,
                    "Unable to initialize due to missing Google Play services "
                    "dependency.");
              }
            }
          },
          data);
    }
#else  // !FIREBASE_PLATFORM_ANDROID
    // Outside of Android, we shouldn't get kInitResultFailedMissingDependency.
    FIREBASE_ASSERT(init_result != kInitResultFailedMissingDependency);
#endif  // FIREBASE_PLATFORM_ANDROID
    if (init_result == kInitResultSuccess) {
      data->init_fn_idx++;  // This function succeeded, move on to the next one.
    } else {
      return;  // If initialization failed, we will be trying again later, after
      // the MakeAvailable() Future completes. (or, if that future fails, then
      // we will just abort and return an error.)
    }
  }
  data->future_impl.Complete(data->future_handle_init, 0);
}

Future<void> ModuleInitializer::Initialize(
    App* app, void* context, ModuleInitializer::InitializerFn init_fn) {
  FIREBASE_ASSERT(app != nullptr);
  FIREBASE_ASSERT(init_fn != nullptr);

  return Initialize(app, context, &init_fn, 1);
}

Future<void> ModuleInitializer::Initialize(
    App* app, void* context, const ModuleInitializer::InitializerFn* init_fns,
    size_t init_fns_count) {
  FIREBASE_ASSERT(app != nullptr);
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

std::map<std::string, AppCallback*>* AppCallback::callbacks_;
Mutex AppCallback::callbacks_mutex_;  // NOLINT

void AppCallback::NotifyAllAppCreated(
    firebase::App* app, std::map<std::string, InitResult>* results) {
  if (results) results->clear();
  MutexLock lock(callbacks_mutex_);
  if (!callbacks_) return;
  for (std::map<std::string, AppCallback*>::const_iterator it =
           callbacks_->begin();
       it != callbacks_->end(); ++it) {
    const AppCallback* callback = it->second;
    if (callback->enabled()) {
      InitResult result = callback->NotifyAppCreated(app);
      if (results) (*results)[it->first] = result;
    }
  }
}

void AppCallback::NotifyAllAppDestroyed(firebase::App* app) {
  MutexLock lock(callbacks_mutex_);
  if (!callbacks_) return;
  for (std::map<std::string, AppCallback*>::const_iterator it =
           callbacks_->begin();
       it != callbacks_->end(); ++it) {
    const AppCallback* callback = it->second;
    if (callback->enabled()) callback->NotifyAppDestroyed(app);
  }
}

// Determine whether a module callback is enabled, by name.
bool AppCallback::GetEnabledByName(const char* name) {
  MutexLock lock(callbacks_mutex_);
  if (!callbacks_) return false;
  std::map<std::string, AppCallback*>::const_iterator it =
      callbacks_->find(std::string(name));
  if (it == callbacks_->end()) {
    return false;
  }
  return it->second->enabled();
}

// Enable a module callback by name.
void AppCallback::SetEnabledByName(const char* name, bool enable) {
  MutexLock lock(callbacks_mutex_);
  if (!callbacks_) return;
  std::map<std::string, AppCallback*>::const_iterator it =
      callbacks_->find(std::string(name));
  if (it == callbacks_->end()) {
    LogDebug("App initializer %s not found, failed to enable.", name);
    return;
  }
  LogDebug("%s app initializer %s", name, enable ? "Enabling" : "Disabling");
  it->second->set_enabled(enable);
}

void AppCallback::SetEnabledAll(bool enable) {
  MutexLock lock(callbacks_mutex_);
  if (!callbacks_) return;
  LogDebug("%s all app initializers", enable ? "Enabling" : "Disabling");
  for (std::map<std::string, AppCallback*>::const_iterator it =
           callbacks_->begin();
       it != callbacks_->end(); ++it) {
    LogDebug("%s %s", enable ? "Enable" : "Disable", it->second->module_name());
    it->second->set_enabled(enable);
  }
}

void AppCallback::AddCallback(AppCallback* callback) {
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

Mutex StaticFutureData::s_futures_mutex_;  // NOLINT
std::map<const void*, StaticFutureData*>* StaticFutureData::s_future_datas_;

// static
void StaticFutureData::CleanupFutureDataForModule(
    const void* module_identifier) {
  MutexLock lock(s_futures_mutex_);

  if (s_future_datas_ == nullptr) return;

  auto it = s_future_datas_->find(module_identifier);
  if (it != s_future_datas_->end()) {
    StaticFutureData* existing_data = it->second;
    if (existing_data != nullptr) delete existing_data;

    s_future_datas_->erase(it);
  }
}

// static
StaticFutureData* StaticFutureData::GetFutureDataForModule(
    const void* module_identifier, int num_functions) {
  MutexLock lock(s_futures_mutex_);

  if (s_future_datas_ == nullptr) {
    s_future_datas_ = new std::map<const void*, StaticFutureData*>;
  }

  auto found_value = s_future_datas_->find(module_identifier);
  if (found_value != s_future_datas_->end()) {
    StaticFutureData* existing_data = found_value->second;
    if (existing_data != nullptr) {
      return existing_data;
    }
  }

  StaticFutureData* new_data = CreateNewData(module_identifier, num_functions);
  (*s_future_datas_)[module_identifier] = new_data;
  return new_data;
}

// static
StaticFutureData* StaticFutureData::CreateNewData(const void* module_identifier,
                                                  int num_functions) {
  StaticFutureData* new_data = new StaticFutureData(num_functions);
  return new_data;
}

std::vector<std::string> SplitString(const std::string& s,
                                     const char delimiter) {
  size_t pos = 0;
  // This index is used as the starting index to search the delimiters from.
  size_t delimiter_search_start = 0;
  // Skip any leading delimiters
  while (s[delimiter_search_start] == delimiter) {
    delimiter_search_start++;
  }

  std::vector<std::string> split_parts;
  size_t len = s.size();
  // Can't proceed if input string consists of just delimiters
  if (pos >= len) {
    return split_parts;
  }

  while ((pos = s.find(delimiter, delimiter_search_start)) !=
         std::string::npos) {
    split_parts.push_back(
        s.substr(delimiter_search_start, pos - delimiter_search_start));

    while (s[pos] == delimiter && pos < len) {
      pos++;
      delimiter_search_start = pos;
    }
  }

  // If the input string doesn't end with a delimiter we need to push the last
  // token into our return vector
  if (delimiter_search_start != len) {
    split_parts.push_back(
        s.substr(delimiter_search_start, len - delimiter_search_start));
  }
  return split_parts;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
