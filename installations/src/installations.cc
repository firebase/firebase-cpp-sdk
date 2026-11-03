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

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    installations,
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::installations::Installations::GetInstance(app);
      }
      return kInitResultSuccess;
    },
    {
      // Cleanup for installations is handled via CleanupNotifier per App.
    },
    false);

namespace firebase {
namespace installations {

namespace {
std::map<App*, Installations*>* g_installations = nullptr;

Mutex& InstallationsMutex() {
  static Mutex* s_installations_mutex = new Mutex();
  return *s_installations_mutex;
}
}  // namespace

using internal::InstallationsInternal;

const char* GetErrorMessage(Error error) noexcept {
  switch (error) {
    case kErrorNone:
      return "";
    case kErrorUnknown:
      return "An unknown Installations error occurred.";
    case kErrorNoAccess:
      return "The Installations service could not be accessed.";
    case kErrorInvalidConfiguration:
      return "Invalid Firebase App or Installations configuration.";
    case kErrorUnableToLoad:
      return "Unable to load or save Installations data in local storage.";
  }
  return "An unknown Installations error occurred.";
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
    if (init_result_out) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }

  Installations* installations = nullptr;
  bool is_new_instance = false;
  {
    MutexLock lock(InstallationsMutex());

    // Return the Installations if it already exists.
    Installations* existing_installations = FindInstallationsLocked(app);
    if (existing_installations) {
      if (init_result_out) {
        *init_result_out = kInitResultSuccess;
      }
      installations = existing_installations;
    } else {
      // Create a new Installations and initialize.
      installations = new Installations(app);
      LogDebug("Creating Installations %p for App %s", installations,
               app->name());

      if (!installations->InitInternal()) {
        delete installations;
        if (init_result_out) {
          *init_result_out = kInitResultFailedMissingDependency;
        }
        return nullptr;
      }

      (*g_installations)[app] = installations;
      is_new_instance = true;
      if (init_result_out) {
        *init_result_out = kInitResultSuccess;
      }
    }
  }

  if (is_new_instance) {
    // Register CleanupNotifier outside InstallationsMutex() to avoid
    // lock-order inversions with CleanupNotifier::CleanupAll().
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
    FIREBASE_ASSERT(notifier);
    if (notifier) {
      notifier->RegisterObject(installations, [](void* object) {
        Installations* installations_local =
            reinterpret_cast<Installations*>(object);
        LogWarning(
            "Installations object %p should be deleted before the App %p it "
            "depends upon.",
            installations_local, installations_local->app());
        installations_local->DeleteInternal();
      });
    }
  } else {
    // Log heartbeat data when instance getters are called on an existing
    // instance (new instances log heartbeat during initialization).
    InstallationsInternal::LogHeartbeat(*app);
  }

  return installations;
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
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_to_unregister);
    if (notifier) {
      notifier->UnregisterObject(this);
    }
  }
}

bool Installations::InitInternal() {
  return installations_internal_ != nullptr &&
         installations_internal_->Initialized();
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
  return installations_internal_ ? installations_internal_->GetIdLastResult()
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
  return installations_internal_ ? installations_internal_->GetTokenLastResult()
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
             ? installations_internal_->GetTokenResultLastResult()
             : Future<InstallationTokenResult>();
}

Future<void> Installations::Delete() {
  MutexLock lock(InstallationsMutex());
  return installations_internal_ ? installations_internal_->Delete()
                                 : Future<void>();
}

Future<void> Installations::DeleteLastResult() const {
  MutexLock lock(InstallationsMutex());
  return installations_internal_ ? installations_internal_->DeleteLastResult()
                                 : Future<void>();
}

}  // namespace installations
}  // namespace firebase
