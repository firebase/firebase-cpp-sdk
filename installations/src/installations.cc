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

#include "app/src/cleanup_notifier.h"
#include "app/src/mutex.h"
#include "installations/src/installations_internal.h"

namespace firebase {
namespace installations {

std::map<App*, Installations*>* g_installations = nullptr;  // NOLINT
Mutex g_installations_mutex;                                // NOLINT

using internal::InstallationsInternal;

Installations* Installations::GetInstance(App* app) {
  MutexLock lock(g_installations_mutex);

  // Return the Installations if it already exists.
  Installations* existing_installations = FindInstallations(app);
  if (existing_installations) {
    return existing_installations;
  }

  // Create a new Installations and initialize.
  Installations* installations = new Installations(app);
  LogDebug("Creating Installations %p for App %s", installations, app->name());

  if (installations->InitInternal()) {
    // Cleanup this object if app is destroyed.
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
    FIREBASE_ASSERT(notifier);
    notifier->RegisterObject(installations, [](void* object) {
      Installations* installations_local =
          reinterpret_cast<Installations*>(object);
      LogWarning(
          "Installations object 0x%08x should be deleted before the App 0x%08x "
          "it depends upon.",
          static_cast<int>(reinterpret_cast<intptr_t>(installations_local)),
          static_cast<int>(
              reinterpret_cast<intptr_t>(installations_local->app())));
      installations_local->DeleteInternal();
    });

    // Stick it in the global map so we remember it, and can delete it on
    // shutdown.
    (*g_installations)[app] = installations;
    return installations;
  }

  return nullptr;
}

Installations* Installations::FindInstallations(App* app) {
  MutexLock lock(g_installations_mutex);
  if (!g_installations) {
    g_installations = new std::map<App*, Installations*>();  // NOLINT
    return nullptr;
  }

  // Return the Installations if it already exists.
  std::map<App*, Installations*>::iterator it =
      g_installations->find(app);  // NOLINT
  if (it != g_installations->end()) {
    return it->second;
  }
  return nullptr;
}

Installations::Installations(App* app) : app_(app) {
  MutexLock lock(g_installations_mutex);
  installations_internal_ = new InstallationsInternal(*app);
}

Installations::~Installations() {
  MutexLock lock(g_installations_mutex);

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_);
  if (notifier) {
    notifier->UnregisterObject(this);
  }

  DeleteInternal();

  if (g_installations) {
    // Remove installations from the g_installations map.
    g_installations->erase(app_);
    if (g_installations->empty()) {
      delete g_installations;
      g_installations = nullptr;
    }
  }
  app_ = nullptr;
}

bool Installations::InitInternal() {
  return installations_internal_->Initialized();
}

void Installations::DeleteInternal() {
  MutexLock lock(g_installations_mutex);
  if (!installations_internal_) return;

  installations_internal_->Cleanup();
  delete installations_internal_;
  installations_internal_ = nullptr;
}

Future<std::string> Installations::GetId() {
  return installations_internal_->GetId();
}

Future<std::string> Installations::GetIdLastResult() {
  return installations_internal_->GetIdLastResult();
}

Future<std::string> Installations::GetToken(bool forceRefresh) {
  return installations_internal_->GetToken(forceRefresh);
}

Future<std::string> Installations::GetTokenLastResult() {
  return installations_internal_->GetTokenLastResult();
}

Future<void> Installations::Delete() {
  return installations_internal_->Delete();
}

Future<void> Installations::DeleteLastResult() {
  return installations_internal_->DeleteLastResult();
}

}  // namespace installations
}  // namespace firebase
