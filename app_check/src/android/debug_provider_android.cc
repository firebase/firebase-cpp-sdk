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
#include "app/src/embedded_file.h"
#include "app/src/util_android.h"
#include "firebase/app_check/debug_provider.h"

namespace firebase {
namespace app_check {
namespace internal {

// Used to setup the cache of DebugProviderFactory class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define DEBUG_PROVIDER_FACTORY_METHODS(X)                                                        \
  X(GetInstance, "getInstance",                                                \
    "()Lcom/google/firebase/appcheck/debug/DebugAppCheckProviderFactory;",  \
       util::kMethodTypeStatic),       \
  X(Create, "create",                                                          \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/AppCheckProvider;")
// clang-format on

METHOD_LOOKUP_DECLARATION(debug_provider_factory, DEBUG_PROVIDER_FACTORY_METHODS)
METHOD_LOOKUP_DEFINITION(debug_provider_factory,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/appcheck/debug/DebugAppCheckProviderFactory",
                         DEBUG_PROVIDER_FACTORY_METHODS)

// TODO: move this to some sort of app check android base class to be reused
// Used to setup the cache of AppCheckProvider interface method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define APP_CHECK_PROVIDER_METHODS(X)                                                        \
  X(GetToken, "getToken",                                                          \
    "()Lcom/google/android/gms/tasks/Task;")
// clang-format on

METHOD_LOOKUP_DECLARATION(app_check_provider, APP_CHECK_PROVIDER_METHODS)
METHOD_LOOKUP_DEFINITION(app_check_provider,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/appcheck/AppCheckProvider.java",
                         APP_CHECK_PROVIDER_METHODS)

/*

TODO: add something like the following for app check debug provider android methods
// the auth one is cached when creating native auth
*/

static bool g_methods_cached = false;

// TODO: better message
const char kMethodsNotCachedError[] =
    "Firebase AppCheck was not initialized, unable to create a Debug provider. "
    "Create an AppCheck instance first.";

bool CacheDebugProviderMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<firebase::internal::EmbeddedFile>& embedded_files) {
  // Cache the DebugProvider classes.
  if (!(debug_provider_factory::CacheClassFromFiles(env, activity, &embedded_files) &&
        debug_provider_factory::CacheMethodIds(env, activity) &&
        app_check_provider::CacheClassFromFiles(env, activity, &embedded_files) &&
        app_check_provider::CacheMethodIds(env, activity))) {
    return false;
  }

  return true;
}

void ReleaseDebugProviderClasses(JNIEnv* env) {
  debug_provider_factory::ReleaseClass(env);
}

// TODO: is this a good approach? can I get JNI env before I have initialized an app?
// I found this method in credential android
static JNIEnv* GetJniEnv() {
  // The JNI environment is the same regardless of App.
  App* app = app_common::GetAnyApp();
  FIREBASE_ASSERT(app != nullptr);
  return app->GetJNIEnv();
}

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  DebugAppCheckProvider(jobject debug_provider);
  virtual ~DebugAppCheckProvider();

  /// Fetches an AppCheckToken and then calls the provided callback method with
  /// the token or with an error code and error message.
  virtual void GetToken(
      std::function<void(AppCheckToken, int, const std::string&)> completion_callback) override;

 private:
  jobject android_provider_; // TODO: make this a jobject DebugProvider
};

/**
 * TODO - if android doesnt expose the individual providers, then a single generic android-provider-wrapper would work
 * can be used for non-debug providers as well
 */
DebugAppCheckProvider::DebugAppCheckProvider(jobject debug_provider)
    : android_provider_(debug_provider) {
        // The global references will be freed when this factory is destroyed
        // TODO: do I need to make the reference global here?
        // android_provider_factory_local = env->NewGlobalRef(j_provider_factory_local);
    }

DebugAppCheckProvider::~DebugAppCheckProvider() {
    JNIEnv* env = GetJniEnv();
  env->DeleteGlobalRef(android_provider_);
}

void DebugAppCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
    JNIEnv* env = GetJniEnv();

  jobject j_task = env->CallObjectMethod(
      android_provider_,
      app_check_provider::GetMethodId(app_check_provider::kGetTokenMethod));
  // TODO: restructure to use task like a future and an oncompletion of the future
  // and catch exception in order to call completion with error code
  assert(env->ExceptionCheck() == false);
  /*
        completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                            firebase::app_check::internal::AppCheckErrorFromNSError(error),
                            util::NSStringToString(error.localizedDescription).c_str());
    */
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : created_providers_() {
    JNIEnv* env = GetJniEnv();
  jobject j_provider_factory_local = env->CallStaticObjectMethod(
    debug_provider_factory::GetClass(), debug_provider_factory::GetMethodId(debug_provider_factory::kGetInstance));

  // Hold a global references.
  // The global references will be freed when this factory is destroyed
  android_provider_factory_ = env->NewGlobalRef(j_provider_factory_local);
}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  for (auto it = created_providers_.begin(); it != created_providers_.end(); ++it) {
    delete it->second;
  }
  created_providers_.clear();
  // Release java global references
  // TODO: should this be conditional?
  JNIEnv* env = GetJniEnv();
  env->DeleteGlobalRef(android_provider_factory_);
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(App* app) {
  // Return the provider if it already exists.
  std::map<App*, AppCheckProvider*>::iterator it = created_providers_.find(app);
  if (it != created_providers_.end()) {
    return it->second;
  }
  // Otherwise, create a new provider
  // TODO: call java create-provider methods
    JNIEnv* env = GetJniEnv();
  jobject j_android_provider_local = env->CallObjectMethod(
    android_provider_factory_, debug_provider_factory::GetMethodId(debug_provider_factory::kCreateMethod),
    app->GetPlatformApp());

  // Pass a global ref into the cpp provider constructor. 
  // It will be released when the cpp provider is destroyed  
  AppCheckProvider* cpp_provider = new internal::DebugAppCheckProvider(
    env->NewGlobalRef(j_android_provider_local));
  created_providers_[app] = cpp_provider;
  return cpp_provider;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
