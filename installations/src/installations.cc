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

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/no_destructor.h"
#include "installations/src/installations_internal.h"

namespace firebase {
namespace installations {

using InstallationsMap = std::map<App*, Installations*>;

// The map and its mutex are function-local statics wrapped in NoDestructor, so
// they are constructed on first use and never destroyed. That keeps them out
// of static initialization and destruction entirely: they are ready regardless
// of translation-unit initialization order, and an Installations instance
// owned by a static object can still lock the mutex and erase itself from its
// destructor during process exit.
static InstallationsMap& InstallationsByApp() {
  static NoDestructor<InstallationsMap> installations;
  return *installations;
}

static Mutex& InstallationsMutex() {
  static NoDestructor<Mutex> mutex;
  return *mutex;
}

using internal::InstallationsInternal;

// Must be called with InstallationsMutex() held.
static Installations* FindInstallationsLocked(App* app) {
  InstallationsMap& installations = InstallationsByApp();
  InstallationsMap::iterator it = installations.find(app);
  if (it != installations.end()) {
    return it->second;
  }
  return nullptr;
}

Installations* Installations::GetInstance(App* app) {
  MutexLock lock(InstallationsMutex());

  // Return the Installations if it already exists.
  Installations* existing_installations = FindInstallationsLocked(app);
  if (existing_installations) {
    // Log heartbeat data when instance getters are called.
    // See go/firebase-platform-logging-design for more information.
    InstallationsInternal::LogHeartbeat(*app);
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

    InstallationsByApp()[app] = installations;
    return installations;
  }

  delete installations;
  return nullptr;
}

Installations* Installations::FindInstallations(App* app) {
  MutexLock lock(InstallationsMutex());
  return FindInstallationsLocked(app);
}

// Private constructor called only from GetInstance() while InstallationsMutex()
// is already held.
Installations::Installations(App* app) : app_(app) {
  FIREBASE_ASSERT(app != nullptr);
  installations_internal_ = new InstallationsInternal(*app);
}

Installations::~Installations() {
  DeleteInternal();
  app_ = nullptr;
}

bool Installations::InitInternal() {
  return installations_internal_->Initialized();
}

void Installations::DeleteInternal() {
  MutexLock lock(InstallationsMutex());
  if (!installations_internal_) return;

  // An instance whose initialization failed was never registered with the
  // App's CleanupNotifier, added to InstallationsByApp(), or left holding
  // JNI/iOS class references.
  if (!installations_internal_->Initialized()) {
    delete installations_internal_;
    installations_internal_ = nullptr;
    return;
  }

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_);
  if (notifier) {
    notifier->UnregisterObject(this);
  }

  installations_internal_->Cleanup();
  delete installations_internal_;
  installations_internal_ = nullptr;

  // Remove `this` from InstallationsByApp() on both App-destruction cleanup and
  // ~Installations() so a recreated App at the same address never finds a
  // gutted Installations shell.
  InstallationsByApp().erase(app_);
}

Future<std::string> Installations::GetId() {
  return installations_internal_ ? installations_internal_->GetId()
                                 : Future<std::string>();
}

Future<std::string> Installations::GetIdLastResult() {
  return installations_internal_ ? installations_internal_->GetIdLastResult()
                                 : Future<std::string>();
}

Future<std::string> Installations::GetToken(bool forceRefresh) {
  return installations_internal_
             ? installations_internal_->GetToken(forceRefresh)
             : Future<std::string>();
}

Future<std::string> Installations::GetTokenLastResult() {
  return installations_internal_ ? installations_internal_->GetTokenLastResult()
                                 : Future<std::string>();
}

Future<void> Installations::Delete() {
  return installations_internal_ ? installations_internal_->Delete()
                                 : Future<void>();
}

Future<void> Installations::DeleteLastResult() {
  return installations_internal_ ? installations_internal_->DeleteLastResult()
                                 : Future<void>();
}

}  // namespace installations
}  // namespace firebase
