// Copyright 2023 Google LLC

#include "firebase/ump/consent_info.h"

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/util.h"
#include "firebase/app.h"
#include "firebase/internal/mutex.h"
#include "firebase/internal/platform.h"
#include "firebase/ump.h"
#include "ump/src/common/consent_info_internal.h"

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
    { (void)app; }, false);

namespace firebase {
namespace ump {

namespace {
Mutex& UmpInstanceMutex() {
  static Mutex* m = new Mutex();
  return *m;
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
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return nullptr;
  }
  ConsentInfo* instance = nullptr;
  {
    MutexLock lock(UmpInstanceMutex());
    if (s_instance_ && s_instance_->internal_) {
#if FIREBASE_PLATFORM_ANDROID
      s_instance_->SetActivity(app->GetJNIEnv(), app->activity());
#endif
      if (init_result_out) *init_result_out = kInitResultSuccess;
      instance = s_instance_;
    } else {
#if FIREBASE_PLATFORM_ANDROID
      instance =
          GetInstance(app->GetJNIEnv(), app->activity(), init_result_out);
#else
      ConsentInfo* ci = s_instance_ ? s_instance_ : new ConsentInfo();
      InitResult res = ci->Initialize();
      if (init_result_out) *init_result_out = res;
      if (res != kInitResultSuccess) {
        if (ci != s_instance_) delete ci;
        return nullptr;
      }
      s_instance_ = instance = ci;
#endif
    }
  }
  if (instance) instance->BindToApp(app);
  return instance;
}

#if FIREBASE_PLATFORM_ANDROID
ConsentInfo* ConsentInfo::GetInstance(::firebase::InitResult* init_result_out) {
  {
    MutexLock lock(UmpInstanceMutex());
    if (s_instance_ && s_instance_->internal_) {
      if (init_result_out) *init_result_out = kInitResultSuccess;
      return s_instance_;
    }
  }
  if (::firebase::App* default_app = ::firebase::App::GetInstance()) {
    return GetInstance(*default_app, init_result_out);
  }
  if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
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
  ConsentInfo* ci = s_instance_ ? s_instance_ : new ConsentInfo();
  InitResult res = ci->Initialize(jni_env, activity);
  if (init_result_out) *init_result_out = res;
  if (res != kInitResultSuccess) {
    if (ci != s_instance_) delete ci;
    return nullptr;
  }
  return s_instance_ = ci;
}

void ConsentInfo::SetActivity(JNIEnv* jni_env, jobject activity) {
  MutexLock lock(UmpInstanceMutex());
  if (internal_) internal_->SetActivity(jni_env, activity);
}
#else
ConsentInfo* ConsentInfo::GetInstance(::firebase::InitResult* init_result_out) {
  ConsentInfo* ci = nullptr;
  {
    MutexLock lock(UmpInstanceMutex());
    if (s_instance_ && s_instance_->internal_) {
      if (init_result_out) *init_result_out = kInitResultSuccess;
      return s_instance_;
    }
    ci = s_instance_ ? s_instance_ : new ConsentInfo();
    InitResult res = ci->Initialize();
    if (init_result_out) *init_result_out = res;
    if (res != kInitResultSuccess) {
      if (ci != s_instance_) delete ci;
      return nullptr;
    }
    s_instance_ = ci;
  }
  if (::firebase::App* default_app = ::firebase::App::GetInstance()) {
    ci->BindToApp(default_app);
  }
  return ci;
}
#endif

ConsentInfo::ConsentInfo() : app_(nullptr), internal_(nullptr) {}

ConsentInfo::~ConsentInfo() {
  ::firebase::App* app_to_unbind = nullptr;
  {
    MutexLock lock(UmpInstanceMutex());
    app_to_unbind = app_;
    app_ = nullptr;
    delete internal_;
    internal_ = nullptr;
    if (s_instance_ == this) s_instance_ = nullptr;
  }
  if (app_to_unbind) {
    if (CleanupNotifier* n = CleanupNotifier::FindByOwner(app_to_unbind)) {
      n->UnregisterObject(this);
    }
  }
}

void ConsentInfo::BindToApp(::firebase::App* app) {
  if (!app) return;
  {
    MutexLock lock(UmpInstanceMutex());
    if (app_) return;
  }
  if (CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app)) {
    {
      MutexLock lock(UmpInstanceMutex());
      if (app_) return;
      app_ = app;
    }
    notifier->RegisterObject(
        this, [](void* obj) { static_cast<ConsentInfo*>(obj)->OnAppDestroyed(); });
  }
}

void ConsentInfo::UnbindFromApp() {
  ::firebase::App* app_to_unbind = nullptr;
  {
    MutexLock lock(UmpInstanceMutex());
    app_to_unbind = app_;
    app_ = nullptr;
  }
  if (app_to_unbind) {
    if (CleanupNotifier* n = CleanupNotifier::FindByOwner(app_to_unbind)) {
      n->UnregisterObject(this);
    }
  }
}

void ConsentInfo::OnAppDestroyed() {
  MutexLock lock(UmpInstanceMutex());
  app_ = nullptr;
  delete internal_;
  internal_ = nullptr;
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

ConsentStatus ConsentInfo::GetConsentStatus() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->GetConsentStatus() : kConsentStatusUnknown;
}

ConsentFormStatus ConsentInfo::GetConsentFormStatus() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->GetConsentFormStatus()
                   : kConsentFormStatusUnknown;
}

Future<void> ConsentInfo::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->RequestConsentInfoUpdate(params)
                   : Future<void>();
}

Future<void> ConsentInfo::RequestConsentInfoUpdateLastResult() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->RequestConsentInfoUpdateLastResult()
                   : Future<void>();
}

Future<void> ConsentInfo::LoadConsentForm() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->LoadConsentForm() : Future<void>();
}

Future<void> ConsentInfo::LoadConsentFormLastResult() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->LoadConsentFormLastResult() : Future<void>();
}

Future<void> ConsentInfo::ShowConsentForm(FormParent parent) {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->ShowConsentForm(parent) : Future<void>();
}

Future<void> ConsentInfo::ShowConsentFormLastResult() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->ShowConsentFormLastResult() : Future<void>();
}

Future<void> ConsentInfo::LoadAndShowConsentFormIfRequired(FormParent parent) {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->LoadAndShowConsentFormIfRequired(parent)
                   : Future<void>();
}

Future<void> ConsentInfo::LoadAndShowConsentFormIfRequiredLastResult() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->LoadAndShowConsentFormIfRequiredLastResult()
                   : Future<void>();
}

PrivacyOptionsRequirementStatus
ConsentInfo::GetPrivacyOptionsRequirementStatus() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->GetPrivacyOptionsRequirementStatus()
                   : kPrivacyOptionsRequirementStatusUnknown;
}

Future<void> ConsentInfo::ShowPrivacyOptionsForm(FormParent parent) {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->ShowPrivacyOptionsForm(parent) : Future<void>();
}

Future<void> ConsentInfo::ShowPrivacyOptionsFormLastResult() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->ShowPrivacyOptionsFormLastResult()
                   : Future<void>();
}

bool ConsentInfo::CanRequestAds() {
  MutexLock lock(UmpInstanceMutex());
  return internal_ ? internal_->CanRequestAds() : false;
}

void ConsentInfo::Reset() {
  MutexLock lock(UmpInstanceMutex());
  if (internal_) internal_->Reset();
}

}  // namespace ump
}  // namespace firebase
