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

#include "app/src/jni/exception.h"
#include "app_check/src/android/debug_provider_android.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/mutex.h"
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
       firebase::util::kMethodTypeStatic),                                               \
  X(Create, "create",                                                          \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/AppCheckProvider;")
// clang-format on

namespace debug_provider_factory {
  enum Method {
    kGetInstance,
    kCreate,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace debug_provider_factory {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/appcheck/debug/DebugAppCheckProviderFactory"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetInstance] = env->GetStaticMethodID(g_class.get(), "getInstance", "()Lcom/google/firebase/appcheck/debug/DebugAppCheckProviderFactory;");
    if (!g_method_ids[kGetInstance]) return false;
    g_method_ids[kCreate] = env->GetMethodID(g_class.get(), "create", "(Lcom/google/firebase/FirebaseApp;)Lcom/google/firebase/appcheck/AppCheckProvider;");
    if (!g_method_ids[kCreate]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


// clang-format off
#define JNI_APP_CHECK_DEBUG_HELPER_METHODS(X)                    \
  X(SetDebugToken, "SetDebugToken",                             \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)V",    \
    firebase::util::kMethodTypeStatic)
// clang-format on

namespace jni_debug_helper {
  enum Method {
    kSetDebugToken,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace jni_debug_helper {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/appcheck/internal/cpp/JniAppCheckDebugHelper"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kSetDebugToken] = env->GetStaticMethodID(g_class.get(), "SetDebugToken", "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)V");
    if (!g_method_ids[kSetDebugToken]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


namespace {

constexpr char kMethodsNotCachedError[] =
    "DebugAppCheckProviderFactory methods were not cached.";

Mutex& GetFactoryMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}

std::vector<const DebugAppCheckProviderFactoryInternal*>& GetLiveFactories() {
  static auto* factories =
      new std::vector<const DebugAppCheckProviderFactoryInternal*>();
  return *factories;
}

bool g_methods_cached = false;

}  // namespace

bool CacheDebugProviderMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<firebase::internal::EmbeddedFile>& embedded_files) {
  MutexLock lock(GetFactoryMutex());
  // Cache the DebugProvider and JniAppCheckDebugHelper classes.
  g_methods_cached =
      debug_provider_factory::CacheMethodIds(env, activity) &&
      jni_debug_helper::CacheClassFromFiles(env, activity, &embedded_files) &&
      jni_debug_helper::CacheMethodIds(env, activity);
  if (!g_methods_cached) {
    debug_provider_factory::ReleaseClass(env);
    jni_debug_helper::ReleaseClass(env);
  }
  return g_methods_cached;
}

void ReleaseDebugProviderClasses(JNIEnv* env) {
  MutexLock lock(GetFactoryMutex());
  debug_provider_factory::ReleaseClass(env);
  jni_debug_helper::ReleaseClass(env);
  g_methods_cached = false;
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : android_provider_factory_(nullptr),
      created_providers_(),
      debug_token_() {
  MutexLock lock(GetFactoryMutex());
  GetLiveFactories().push_back(this);
}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  // Note: created_providers_ is declared as std::map<App*, AppCheckProvider*>
  // in debug_provider_android.h; ownership is managed via std::unique_ptr on
  // creation and eviction.
  std::map<App*, AppCheckProvider*> providers;
  jobject factory_ref = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    auto& live = GetLiveFactories();
    live.erase(std::remove(live.begin(), live.end(), this), live.end());
    providers.swap(created_providers_);
    factory_ref = android_provider_factory_;
    android_provider_factory_ = nullptr;
  }
  for (auto& [app, raw_provider] : providers) {
    std::unique_ptr<AppCheckProvider> provider(raw_provider);
;
    if (notifier != nullptr && provider != nullptr) {
      notifier->UnregisterObject(provider.get());
    }
  }
  JNIEnv* env = GetJniEnv();
  if (env != nullptr && factory_ref != nullptr) {
    env->DeleteGlobalRef(factory_ref);
  }
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  if (!app) return nullptr;
  AppCheckProvider* raw_provider = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    FIREBASE_ASSERT_MESSAGE_RETURN(nullptr, g_methods_cached,
                                   kMethodsNotCachedError);

    // Return the provider if it already exists.
    auto it = created_providers_.find(app);
    if (it != created_providers_.end()) {
      return it->second;
    }
    JNIEnv* env = app->GetJNIEnv();
    if (env == nullptr) {
      return nullptr;
    }
    // Create a provider factory first if needed.
    if (android_provider_factory_ == nullptr) {
      jobject j_provider_factory_local =
          env->CallStaticObjectMethod(debug_provider_factory::GetClass(),
                                      debug_provider_factory::GetMethodId(
                                          debug_provider_factory::kGetInstance));
      if (jni::CheckAndClearException(env) ||
          j_provider_factory_local == nullptr) {
        if (j_provider_factory_local != nullptr) {
          env->DeleteLocalRef(j_provider_factory_local);
        }
        return nullptr;
      }
      // Hold a global reference. It will be freed when this factory is destroyed.
      android_provider_factory_ = env->NewGlobalRef(j_provider_factory_local);
      env->DeleteLocalRef(j_provider_factory_local);
      if (jni::CheckAndClearException(env) ||
          android_provider_factory_ == nullptr) {
        android_provider_factory_ = nullptr;
        return nullptr;
      }
    }
    jobject platform_app = app->GetPlatformApp();
    if (platform_app == nullptr) {
      return nullptr;
    }

    // Before making the Provider, which is when Android reads the Debug token,
    // if we have a token to override it with, use it.
    if (!debug_token_.empty()) {
      jstring java_token = env->NewStringUTF(debug_token_.c_str());
      if (!jni::CheckAndClearException(env) && java_token != nullptr) {
        env->CallStaticVoidMethod(
            jni_debug_helper::GetClass(),
            jni_debug_helper::GetMethodId(jni_debug_helper::kSetDebugToken),
            platform_app, java_token);
        jni::CheckAndClearException(env);
        env->DeleteLocalRef(java_token);
      }
    }

    jobject j_android_provider_local = env->CallObjectMethod(
        android_provider_factory_,
        debug_provider_factory::GetMethodId(debug_provider_factory::kCreate),
        platform_app);
    bool create_exception = jni::CheckAndClearException(env);
    env->DeleteLocalRef(platform_app);
    if (create_exception || j_android_provider_local == nullptr) {
      if (j_android_provider_local != nullptr) {
        env->DeleteLocalRef(j_android_provider_local);
      }
      return nullptr;
    }

    // Pass a local ref into the C++ provider constructor; it creates its own
    // global ref and releases it when the C++ provider is destroyed.
    auto cpp_provider =
        std::make_unique<internal::AndroidAppCheckProvider>(
            j_android_provider_local);
    env->DeleteLocalRef(j_android_provider_local);
    raw_provider = cpp_provider.release();
    created_providers_[app] = raw_provider;
  }

;
  if (notifier != nullptr) {
    notifier->RegisterObject(
        raw_provider, [this, app, raw_provider](void* /*object*/) {
          std::unique_ptr<AppCheckProvider> provider_to_delete;
          {
            MutexLock cleanup_lock(GetFactoryMutex());
            const auto& live = GetLiveFactories();
            if (std::find(live.begin(), live.end(), this) == live.end()) {
              return;
            }
            auto provider_it = created_providers_.find(app);
            if (provider_it != created_providers_.end() &&
                provider_it->second == raw_provider) {
              provider_to_delete.reset(provider_it->second);
              created_providers_.erase(provider_it);
            }
          }
        });
  }
  return raw_provider;
}

void DebugAppCheckProviderFactoryInternal::SetDebugToken(
    const char* token) {
  MutexLock lock(GetFactoryMutex());
  // The App may not exist yet, so we save the debug token to use later.
  debug_token_ = token ? token : "";
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
