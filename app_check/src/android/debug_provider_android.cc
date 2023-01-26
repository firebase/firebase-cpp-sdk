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

#include "app/src/app_common.h"
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

static const char* kApiIdentifier = "DebugAppCheckProvider";

bool CacheDebugProviderMethodIds(
    JNIEnv* env, jobject activity) {
  // Cache the DebugProvider classes.
  if (!debug_provider_factory::CacheMethodIds(env, activity)) {
    return false;
  }

  return true;
}

void ReleaseDebugProviderClasses(JNIEnv* env) {
  debug_provider_factory::ReleaseClass(env);
}

// TODO: is this a good approach? can I get JNI env before I have initialized an
// app? I found this method in credential android
// Alternatively I can have a non-static method that uses app_
static JNIEnv* GetJniEnv() {
  // The JNI environment is the same regardless of App.
  App* app = app_common::GetAnyApp();
  FIREBASE_ASSERT(app != nullptr);
  return app->GetJNIEnv();
}

namespace {

// TODO - better name for this thing and the variables of this type. callback wrapper
struct FutureCallbackData {
  FutureCallbackData(std::function<void(AppCheckToken, int, const std::string&)> callback_)
      : callback(callback_) {}
  std::function<void(AppCheckToken, int, const std::string&)> callback;
};

}  // namespace

void TokenResultCallback(JNIEnv* env, jobject result,
                          util::FutureResult result_code,
                          const char* status_message, void* callback_data) {
  int result_error_code = kAppCheckErrorNone;
  AppCheckToken result_token;
  bool success = (result_code == util::kFutureResultSuccess);
  std::string result_value = "";
  if (success && result) {
    result_token = CppTokenFromAndroidToken(env, result);
  } else {
    // Using error code unknown for failure because android appcheck doesnt have an error code enum
    result_error_code = kAppCheckErrorUnknown;
  }
  auto* completion_callback_data = reinterpret_cast<FutureCallbackData*>(callback_data);
 
  // Evaluate the callback method 
  completion_callback_data->callback(result_token, result_error_code, status_message);
  delete completion_callback_data;
}

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  DebugAppCheckProvider(jobject debug_provider);
  virtual ~DebugAppCheckProvider();

  /// Fetches an AppCheckToken and then calls the provided callback method with
  /// the token or with an error code and error message.
  virtual void GetToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) override;

 private:
  jobject android_provider_;  // TODO: make this a jobject DebugProvider
};

/**
 * TODO - if android doesnt expose the individual providers, then a single
 * generic android-provider-wrapper would work can be used for non-debug
 * providers as well
 * 
 * if so, move this to common_android
 */
DebugAppCheckProvider::DebugAppCheckProvider(jobject local_debug_provider)
    : android_provider_(nullptr) {
  JNIEnv* env = GetJniEnv();
  android_provider_ = env->NewGlobalRef(local_debug_provider);
}

DebugAppCheckProvider::~DebugAppCheckProvider() {
  JNIEnv* env = GetJniEnv();
  if (env != nullptr && android_provider_ != nullptr) {
    env->DeleteGlobalRef(android_provider_);
  }
}

void DebugAppCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  JNIEnv* env = GetJniEnv();

  jobject j_task = env->CallObjectMethod(
      android_provider_,
      app_check_provider::GetMethodId(app_check_provider::kGetToken));
  // TODO - maybe use AndroidHelper::CheckJNIException() {
  // it is unclear if task exception would show up here or would show up in the result of the task
  // might want to call the completion callback here for some errors
  assert(env->ExceptionCheck() == false);

  // Create an object to wrap the callback function
  FutureCallbackData* completion_callback_data = new FutureCallbackData(completion_callback);

  util::RegisterCallbackOnTask(env, j_task, TokenResultCallback,
                               reinterpret_cast<void*>(completion_callback_data),
                               kApiIdentifier);
  
  env->DeleteLocalRef(j_task);
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : created_providers_() {
  // TODO: almostmatt - was this actually needed? try without it
  android_provider_factory_ = nullptr;
}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  for (auto it = created_providers_.begin(); it != created_providers_.end();
       ++it) {
    delete it->second;
  }
  created_providers_.clear();
  // Release java global references
  // TODO: should this be conditional?
  JNIEnv* env = GetJniEnv();
  if (env != nullptr && android_provider_factory_ != nullptr) {
    env->DeleteGlobalRef(android_provider_factory_);
  }
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  // Return the provider if it already exists.
  std::map<App*, AppCheckProvider*>::iterator it = created_providers_.find(app);
  if (it != created_providers_.end()) {
    return it->second;
  }
  JNIEnv* env = GetJniEnv();
  // Create a provider factory first if needed.
  if (android_provider_factory_ == nullptr) {
    jobject j_provider_factory_local =
        env->CallStaticObjectMethod(debug_provider_factory::GetClass(),
                                    debug_provider_factory::GetMethodId(
                                        debug_provider_factory::kGetInstance));
    FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
    // Hold a global references.
    // The global references will be freed when this factory is destroyed
    android_provider_factory_ = env->NewGlobalRef(j_provider_factory_local);
    env->DeleteLocalRef(j_provider_factory_local);
  }
  jobject platform_app = app->GetPlatformApp();
  jobject j_android_provider_local =
      env->CallObjectMethod(android_provider_factory_,
                            debug_provider_factory::GetMethodId(
                                debug_provider_factory::kCreate),
                            platform_app);
  FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
  env->DeleteLocalRef(platform_app);

  // Pass a global ref into the cpp provider constructor.
  // It will be released when the cpp provider is destroyed
  AppCheckProvider* cpp_provider = new internal::DebugAppCheckProvider(
      j_android_provider_local);
  env->DeleteLocalRef(j_android_provider_local);
  created_providers_[app] = cpp_provider;
  return cpp_provider;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
