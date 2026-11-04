// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "installations/src/include/firebase/installations.h"

#include <cstdint>
#include <map>
#include <string>

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "installations/src/installations_internal.h"

FIREBASE_APP_REGISTER_CALLBACKS(
    installations,
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::installations::Installations::GetInstance(app);
      }
      return kInitResultSuccess;
    },
    {}, false);

namespace firebase {
namespace installations {

namespace {
std::map<App*, Installations*>* g_installations = nullptr;
Mutex& InstallationsMutex() {
  static auto* m = new Mutex();
  return *m;
}
}  // namespace

using internal::InstallationsInternal;

const char* GetErrorMessage(Error error) noexcept {
  switch (error) {
    case kErrorNone:
      return "";
    case kErrorNoAccess:
      return "The Installations service could not be accessed.";
    case kErrorInvalidConfiguration:
      return "Invalid Firebase App or Installations configuration.";
    case kErrorUnableToLoad:
      return "Unable to load or save Installations data in local storage.";
    case kErrorUnknown:
    default:
      return "An unknown Installations error occurred.";
  }
}

Installations* Installations::GetInstance() {
  App* default_app = App::GetInstance();
  if (!default_app) {
    LogError("Installations::GetInstance() called with no default App.");
    return nullptr;
  }
  return GetInstance(default_app, nullptr);
}

Installations* Installations::GetInstance(App* app) {
  return GetInstance(app, nullptr);
}

Installations* Installations::GetInstance(App* app,
                                          InitResult* init_result_out) {
  if (!app) {
    LogError("Installations::GetInstance() called with nullptr App.");
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return nullptr;
  }

  Installations* inst = nullptr;
  Installations* failed = nullptr;
  bool is_new = false;
  {
    MutexLock lock(InstallationsMutex());
    if (Installations* existing = FindInstallationsLocked(app)) {
      if (init_result_out) *init_result_out = kInitResultSuccess;
      inst = existing;
    } else {
      inst = new Installations(app);
      if (!inst->InitInternal()) {
        inst->DeleteInternalLocked();
        failed = inst;
        inst = nullptr;
        if (init_result_out)
          *init_result_out = kInitResultFailedMissingDependency;
      } else {
        (*g_installations)[app] = inst;
        is_new = true;
        if (init_result_out) *init_result_out = kInitResultSuccess;
      }
    }
  }
  if (failed) {
    delete failed;
    return nullptr;
  }
  if (is_new) {
    if (CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app)) {
      notifier->RegisterObject(inst, [](void* object) {
        auto* local = reinterpret_cast<Installations*>(object);
        LogWarning(
            "Installations object 0x%08x should be deleted before the App "
            "0x%08x it depends upon.",
            static_cast<int>(reinterpret_cast<intptr_t>(local)),
            static_cast<int>(reinterpret_cast<intptr_t>(local->app())));
        local->DeleteInternal();
      });
    }
  } else {
    InstallationsInternal::LogHeartbeat(*app);
  }
  return inst;
}

Installations* Installations::FindInstallationsLocked(App* app) {
  if (!g_installations) {
    g_installations = new std::map<App*, Installations*>();
    return nullptr;
  }
  auto it = g_installations->find(app);
  return it != g_installations->end() ? it->second : nullptr;
}

Installations* Installations::FindInstallations(App* app) {
  MutexLock lock(InstallationsMutex());
  return FindInstallationsLocked(app);
}

Installations::Installations(App* app)
    : app_(app), installations_internal_(new InstallationsInternal(*app)) {}

Installations::~Installations() {
  App* app_to_unregister = nullptr;
  {
    MutexLock lock(InstallationsMutex());
    app_to_unregister = app_;
    DeleteInternalLocked();
  }
  if (app_to_unregister) {
    if (CleanupNotifier* notifier =
            CleanupNotifier::FindByOwner(app_to_unregister)) {
      notifier->UnregisterObject(this);
    }
  }
}

bool Installations::InitInternal() {
  return installations_internal_ && installations_internal_->Initialized();
}

void Installations::DeleteInternalLocked() {
  if (installations_internal_) {
    installations_internal_->Cleanup();
    delete installations_internal_;
    installations_internal_ = nullptr;
  }
  if (app_ && g_installations) {
    g_installations->erase(app_);
    if (g_installations->empty()) {
      delete g_installations;
      g_installations = nullptr;
    }
  }
  app_ = nullptr;
}

void Installations::DeleteInternal() {
  MutexLock lock(InstallationsMutex());
  DeleteInternalLocked();
}

Future<std::string> Installations::GetId() {
  MutexLock lock(InstallationsMutex());
  return installations_internal_ ? installations_internal_->GetId()
                                 : Future<std::string>();
}

Future<std::string> Installations::GetIdLastResult() const {
  MutexLock lock(InstallationsMutex());
  return installations_internal_
             ? installations_internal_->futures()
                   .LastResult<internal::kInstallationsFnGetId>()
             : Future<std::string>();
}

Future<std::string> Installations::GetToken(bool force_refresh) {
  MutexLock lock(InstallationsMutex());
  return installations_internal_
             ? installations_internal_->GetToken(force_refresh)
             : Future<std::string>();
}

Future<std::string> Installations::GetTokenLastResult() const {
  MutexLock lock(InstallationsMutex());
  return installations_internal_
             ? installations_internal_->futures()
                   .LastResult<internal::kInstallationsFnGetToken>()
             : Future<std::string>();
}

Future<InstallationTokenResult> Installations::GetTokenResult(
    bool force_refresh) {
  MutexLock lock(InstallationsMutex());
  return installations_internal_
             ? installations_internal_->GetTokenResult(force_refresh)
             : Future<InstallationTokenResult>();
}

Future<InstallationTokenResult> Installations::GetTokenResultLastResult()
    const {
  MutexLock lock(InstallationsMutex());
  return installations_internal_
             ? installations_internal_->futures()
                   .LastResult<internal::kInstallationsFnGetTokenResult>()
             : Future<InstallationTokenResult>();
}

Future<void> Installations::Delete() {
  MutexLock lock(InstallationsMutex());
  return installations_internal_ ? installations_internal_->Delete()
                                 : Future<void>();
}

Future<void> Installations::DeleteLastResult() const {
  MutexLock lock(InstallationsMutex());
  return installations_internal_
             ? installations_internal_->futures()
                   .LastResult<internal::kInstallationsFnDelete>()
             : Future<void>();
}

}  // namespace installations
}  // namespace firebase
