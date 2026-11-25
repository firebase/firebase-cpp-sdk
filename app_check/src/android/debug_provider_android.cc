// Copyright 2022 Google LLC
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

#include "app_check/src/android/debug_provider_android.h"

#include <memory>
#include <string>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/app_state.h"
#include "app/src/embedded_file.h"
#include "app/src/log.h"
#include "app/src/service_interfaces.h"
#include "app/src/util_android.h"
#include "app_check/src/android/common_android.h"
#include "firebase/app_check/debug_provider.h"

namespace firebase {
namespace app_check {
namespace internal {

// Used to setup the cache of DebugProviderFactory class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define DEBUG_PROVIDER_FACTORY_METHODS(X)                                      \
  X(GetInstance, "getInstance",                                                \
    "()Lcom/google/firebase/appcheck/debug/DebugAppCheckProviderFactory;",     \
       util::kMethodTypeStatic),                                               \
  X(Create, "create",                                                          \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/AppCheckProvider;")
// clang-format on

METHOD_LOOKUP_DECLARATION(debug_provider_factory,
                          DEBUG_PROVIDER_FACTORY_METHODS)
METHOD_LOOKUP_DEFINITION(
    debug_provider_factory,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/appcheck/debug/DebugAppCheckProviderFactory",
    DEBUG_PROVIDER_FACTORY_METHODS)

// clang-format off
#define JNI_APP_CHECK_DEBUG_HELPER_METHODS(X)                    \
  X(SetDebugToken, "SetDebugToken",                             \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)V",    \
    util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(jni_debug_helper, JNI_APP_CHECK_DEBUG_HELPER_METHODS)
METHOD_LOOKUP_DEFINITION(
    jni_debug_helper,
    "com/google/firebase/appcheck/internal/cpp/JniAppCheckDebugHelper",
    JNI_APP_CHECK_DEBUG_HELPER_METHODS)

static bool g_methods_cached = false;

const char kMethodsNotCachedError[] =
    "DebugAppCheckProviderFactory methods were not cached.";

bool CacheDebugProviderMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<firebase::internal::EmbeddedFile>& embedded_files) {
  // Cache the DebugProvider and JniAppCheckDebugHelper classes.
  g_methods_cached =
      debug_provider_factory::CacheMethodIds(env, activity) &&
      jni_debug_helper::CacheClassFromFiles(env, activity, &embedded_files) &&
      jni_debug_helper::CacheMethodIds(env, activity);
  return g_methods_cached;
}

void ReleaseDebugProviderClasses(JNIEnv* env) {
  debug_provider_factory::ReleaseClass(env);
  jni_debug_helper::ReleaseClass(env);
  g_methods_cached = false;
}

namespace {

const char kAndroidDebugProviderServiceTag = 0;

class AndroidDebugProviderService : public ::firebase::internal::Service {
 public:
  explicit AndroidDebugProviderService(jobject local_provider)
      : provider_(local_provider) {}

  int teardown_priority() const override {
    return ::firebase::internal::kProviderSubServiceTeardownPriority;
  }

  AndroidAppCheckProvider* provider() { return &provider_; }

 private:
  AndroidAppCheckProvider provider_;
};

}  // namespace

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : android_provider_factory_(nullptr), debug_token_() {}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  JNIEnv* env = firebase::util::GetJNIEnvFromApp();
  if (env != nullptr && android_provider_factory_ != nullptr) {
    env->DeleteGlobalRef(android_provider_factory_);
    android_provider_factory_ = nullptr;
  }
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    const App& app) {
  FIREBASE_ASSERT_MESSAGE_RETURN(nullptr, g_methods_cached,
                                 kMethodsNotCachedError);
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) return nullptr;

  firebase::LogWarning(
      "[FirebaseAppCheck] DebugAppCheckProviderFactory is in use. Debug tokens "
      "are for development only; prefer a production provider where one is "
      "available.");

  auto service = state->GetOrCreateService<AndroidDebugProviderService>(
      &kAndroidDebugProviderServiceTag, "",
      [this, &app](::firebase::internal::AppState&) {
        JNIEnv* env = app.GetJNIEnv();
        jobject factory_ref = nullptr;
        std::string token;
        {
          MutexLock lock(mutex_);
          if (android_provider_factory_ == nullptr) {
            jobject j_provider_factory_local = env->CallStaticObjectMethod(
                debug_provider_factory::GetClass(),
                debug_provider_factory::GetMethodId(
                    debug_provider_factory::kGetInstance));
            FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
            android_provider_factory_ =
                env->NewGlobalRef(j_provider_factory_local);
            env->DeleteLocalRef(j_provider_factory_local);
          }
          factory_ref = android_provider_factory_;
          token = debug_token_;
        }

        jobject platform_app = app.GetPlatformApp();
        if (!token.empty()) {
          jstring java_token = env->NewStringUTF(token.c_str());
          env->CallStaticVoidMethod(
              jni_debug_helper::GetClass(),
              jni_debug_helper::GetMethodId(jni_debug_helper::kSetDebugToken),
              platform_app, java_token);
          FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
          env->DeleteLocalRef(java_token);
        }

        jobject j_android_provider_local =
            env->CallObjectMethod(factory_ref,
                                  debug_provider_factory::GetMethodId(
                                      debug_provider_factory::kCreate),
                                  platform_app);
        FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
        env->DeleteLocalRef(platform_app);

        auto created = std::make_shared<AndroidDebugProviderService>(
            j_android_provider_local);
        env->DeleteLocalRef(j_android_provider_local);
        return created;
      });
  return service ? service->provider() : nullptr;
}

void DebugAppCheckProviderFactoryInternal::SetDebugToken(
    const std::string& token) {
  MutexLock lock(mutex_);
  // The App may not exist yet, so we save the debug token to use later.
  debug_token_ = token;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
