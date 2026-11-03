/*
 * Copyright 2023 Google LLC
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

#include "firebase/ump/consent_info.h"

#include <cassert>

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/util.h"
#include "firebase/app.h"
#include "firebase/internal/mutex.h"
#include "firebase/internal/platform.h"
#include "firebase/ump.h"
#include "ump/src/common/consent_info_internal.h"

// Register the module initializer for UMP.
FIREBASE_APP_REGISTER_CALLBACKS(
    ump,
    {
      if (app == ::firebase::App::GetInstance()) {
        ::firebase::InitResult init_result = ::firebase::kInitResultSuccess;
        ::firebase::ump::ConsentInfo::GetInstance(*app, &init_result);
        return init_result;
      }
      return ::firebase::kInitResultSuccess;
    },
    {
      // Cleanup is handled by CleanupNotifier when bound to an App.
      (void)app;
    },
    false);

namespace firebase {
namespace ump {

namespace {
Mutex& UmpInstanceMutex() {
  static Mutex* g_ump_instance_mutex = new Mutex();
  return *g_ump_instance_mutex;
}
}  // namespace

ConsentInfo* ConsentInfo::s_instance_ = nullptr;

ConsentInfo* ConsentInfo::GetInstance(const ::firebase::App& app,
                                      ::firebase::InitResult* init_result_out) {
  return GetInstance(const_cast<::firebase::App*>(&app), init_result_out);
}

ConsentInfo* ConsentInfo::GetInstance(::firebase::App* app,
                                      ::firebase::InitResult* init_result_out) {
  if (!app) {
    if (init_result_out) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }

  MutexLock lock(UmpInstanceMutex());
  if (s_instance_ && s_instance_->internal_) {
#if FIREBASE_PLATFORM_ANDROID
    s_instance_->SetActivity(app->GetJNIEnv(), app->activity());
#endif
    if (!s_instance_->app_) {
      s_instance_->BindToApp(app);
    }
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return s_instance_;
  }

#if FIREBASE_PLATFORM_ANDROID
  ConsentInfo* instance =
      GetInstance(app->GetJNIEnv(), app->activity(), init_result_out);
#else
  ConsentInfo* instance = GetInstance(init_result_out);
#endif
  if (instance) {
    instance->BindToApp(app);
  }
  return instance;
}

#if FIREBASE_PLATFORM_ANDROID
ConsentInfo* ConsentInfo::GetInstance(::firebase::InitResult* init_result_out) {
  MutexLock lock(UmpInstanceMutex());
  if (s_instance_ && s_instance_->internal_) {
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return s_instance_;
  }
  ::firebase::App* default_app = ::firebase::App::GetInstance();
  if (default_app) {
    return GetInstance(*default_app, init_result_out);
  }
  if (init_result_out) {
    *init_result_out = kInitResultFailedMissingDependency;
  }
  return nullptr;
}

ConsentInfo* ConsentInfo::GetInstance(JNIEnv* jni_env, jobject activity,
                                      ::firebase::InitResult* init_result_out) {
  MutexLock lock(UmpInstanceMutex());
  if (s_instance_ && s_instance_->internal_) {
    s_instance_->SetActivity(jni_env, activity);
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return s_instance_;
  }

  ConsentInfo* consent_info = s_instance_ ? s_instance_ : new ConsentInfo();
  InitResult result = consent_info->Initialize(jni_env, activity);
  if (init_result_out) *init_result_out = result;
  if (result != kInitResultSuccess) {
    if (consent_info != s_instance_) {
      delete consent_info;
    }
    return nullptr;
  }
  s_instance_ = consent_info;
  return consent_info;
}

void ConsentInfo::SetActivity(JNIEnv* jni_env, jobject activity) {
  MutexLock lock(UmpInstanceMutex());
  if (internal_) {
    internal_->SetActivity(jni_env, activity);
  }
}
#else   // !FIREBASE_PLATFORM_ANDROID
ConsentInfo* ConsentInfo::GetInstance(::firebase::InitResult* init_result_out) {
  MutexLock lock(UmpInstanceMutex());
  if (s_instance_ && s_instance_->internal_) {
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return s_instance_;
  }

  ConsentInfo* consent_info = s_instance_ ? s_instance_ : new ConsentInfo();
  InitResult result = consent_info->Initialize();
  if (init_result_out) *init_result_out = result;
  if (result != kInitResultSuccess) {
    if (consent_info != s_instance_) {
      delete consent_info;
    }
    return nullptr;
  }
  s_instance_ = consent_info;
  ::firebase::App* default_app = ::firebase::App::GetInstance();
  if (default_app && !s_instance_->app_) {
    s_instance_->BindToApp(default_app);
  }
  return consent_info;
}
#endif  // FIREBASE_PLATFORM_ANDROID

ConsentInfo::ConsentInfo() : app_(nullptr), internal_(nullptr) {}

ConsentInfo::~ConsentInfo() {
  ::firebase::App* app_to_unbind = nullptr;
  {
    MutexLock lock(UmpInstanceMutex());
    app_to_unbind = app_;
    app_ = nullptr;
    if (internal_) {
      delete internal_;
      internal_ = nullptr;
    }
    if (s_instance_ == this) {
      s_instance_ = nullptr;
    }
  }
  if (app_to_unbind) {
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_to_unbind);
    if (notifier) {
      notifier->UnregisterObject(this);
    }
  }
}

void ConsentInfo::BindToApp(::firebase::App* app) {
  if (!app || app_) return;
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  if (notifier) {
    app_ = app;
    notifier->RegisterObject(this, [](void* object) {
      ConsentInfo* info = static_cast<ConsentInfo*>(object);
      info->OnAppDestroyed();
    });
  }
}

void ConsentInfo::UnbindFromApp() {
  ::firebase::App* app_to_unbind = nullptr;
  {
    MutexLock lock(UmpInstanceMutex());
    app_to_unbind = app_;
    app_ = nullptr;
  }
  if (!app_to_unbind) return;
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_to_unbind);
  if (notifier) {
    notifier->UnregisterObject(this);
  }
}

void ConsentInfo::OnAppDestroyed() {
  MutexLock lock(UmpInstanceMutex());
  app_ = nullptr;
  if (internal_) {
    delete internal_;
    internal_ = nullptr;
  }
  // Keep s_instance_ pointing to the hollowed-out ConsentInfo shell so
  // subsequent GetInstance() calls reuse this allocation without leaking, and
  // any existing ConsentInfo* pointers held by callers safely no-op.
}

#if FIREBASE_PLATFORM_ANDROID
InitResult ConsentInfo::Initialize(JNIEnv* jni_env, jobject activity) {
  FIREBASE_ASSERT(!internal_);
  internal_ = internal::ConsentInfoInternal::CreateInstance(jni_env, activity);
  return internal_ ? kInitResultSuccess : kInitResultFailedMissingDependency;
}
#else
InitResult ConsentInfo::Initialize() {
  FIREBASE_ASSERT(!internal_);
  internal_ = internal::ConsentInfoInternal::CreateInstance();
  return internal_ ? kInitResultSuccess : kInitResultFailedMissingDependency;
}
#endif

// Below this, everything is a passthrough to ConsentInfoInternal. If there is
// no internal_ pointer (e.g. it's been cleaned up), return default values and
// invalid futures.

ConsentStatus ConsentInfo::GetConsentStatus() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return kConsentStatusUnknown;
  return internal_->GetConsentStatus();
}

ConsentFormStatus ConsentInfo::GetConsentFormStatus() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return kConsentFormStatusUnknown;
  return internal_->GetConsentFormStatus();
}

Future<void> ConsentInfo::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->RequestConsentInfoUpdate(params);
}

Future<void> ConsentInfo::RequestConsentInfoUpdateLastResult() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->RequestConsentInfoUpdateLastResult();
}

Future<void> ConsentInfo::LoadConsentForm() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->LoadConsentForm();
}

Future<void> ConsentInfo::LoadConsentFormLastResult() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->LoadConsentFormLastResult();
}

Future<void> ConsentInfo::ShowConsentForm(FormParent parent) {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->ShowConsentForm(parent);
}

Future<void> ConsentInfo::ShowConsentFormLastResult() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->ShowConsentFormLastResult();
}

Future<void> ConsentInfo::LoadAndShowConsentFormIfRequired(FormParent parent) {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->LoadAndShowConsentFormIfRequired(parent);
}

Future<void> ConsentInfo::LoadAndShowConsentFormIfRequiredLastResult() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->LoadAndShowConsentFormIfRequiredLastResult();
}

PrivacyOptionsRequirementStatus
ConsentInfo::GetPrivacyOptionsRequirementStatus() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return kPrivacyOptionsRequirementStatusUnknown;
  return internal_->GetPrivacyOptionsRequirementStatus();
}

Future<void> ConsentInfo::ShowPrivacyOptionsForm(FormParent parent) {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->ShowPrivacyOptionsForm(parent);
}

Future<void> ConsentInfo::ShowPrivacyOptionsFormLastResult() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return Future<void>();
  return internal_->ShowPrivacyOptionsFormLastResult();
}

bool ConsentInfo::CanRequestAds() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return false;
  return internal_->CanRequestAds();
}

void ConsentInfo::Reset() {
  MutexLock lock(UmpInstanceMutex());
  if (!internal_) return;
  internal_->Reset();
}

}  // namespace ump
}  // namespace firebase
