// Copyright 2017 Google LLC
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

#include "functions/src/android/functions_android.h"

#include <jni.h>
#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "functions/src/android/callable_reference_android.h"

namespace firebase {
namespace functions {
namespace internal {

const char kApiIdentifier[] = "Functions";

// clang-format off
#define FIREBASE_FUNCTIONS_METHODS(X)                                   \
  X(GetInstance, "getInstance",                                         \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"             \
    "Lcom/google/firebase/functions/FirebaseFunctions;",                \
    util::kMethodTypeStatic),                                           \
  X(GetHttpsCallable, "getHttpsCallable",                               \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/functions/HttpsCallableReference;",           \
    util::kMethodTypeInstance),                                          \
  X(UseFunctionsEmulator, "useFunctionsEmulator",                       \
    "(Ljava/lang/String;)V",                                            \
    util::kMethodTypeInstance)
// clang-format on

METHOD_LOOKUP_DECLARATION(firebase_functions, FIREBASE_FUNCTIONS_METHODS)
METHOD_LOOKUP_DEFINITION(firebase_functions,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/functions/FirebaseFunctions",
                         FIREBASE_FUNCTIONS_METHODS)

// clang-format off
#define FUNCTIONS_EXCEPTION_METHODS(X)                                    \
  X(GetMessage, "getMessage", "()Ljava/lang/String;"),                    \
  X(GetCode, "getCode",                                                   \
    "()Lcom/google/firebase/functions/FirebaseFunctionsException$Code;")
// clang-format on

METHOD_LOOKUP_DECLARATION(functions_exception, FUNCTIONS_EXCEPTION_METHODS)
METHOD_LOOKUP_DEFINITION(
    functions_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/functions/FirebaseFunctionsException",
    FUNCTIONS_EXCEPTION_METHODS)

// clang-format off
#define FUNCTIONS_EXCEPTION_CODE_METHODS(X)                                    \
  X(Ordinal, "ordinal", "()I")
// clang-format on

METHOD_LOOKUP_DECLARATION(functions_exception_code,
                          FUNCTIONS_EXCEPTION_CODE_METHODS)
METHOD_LOOKUP_DEFINITION(
    functions_exception_code,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/functions/FirebaseFunctionsException$Code",
    FUNCTIONS_EXCEPTION_CODE_METHODS)

Mutex FunctionsInternal::init_mutex_;  // NOLINT
int FunctionsInternal::initialize_count_ = 0;

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : region_(region) {
  app_ = nullptr;
  if (!Initialize(app)) return;
  app_ = app;
  JNIEnv* env = app_->GetJNIEnv();
  jstring region_str = env->NewStringUTF(region);
  jobject platform_app = app_->GetPlatformApp();
  jobject functions_obj = env->CallStaticObjectMethod(
      firebase_functions::GetClass(),
      firebase_functions::GetMethodId(firebase_functions::kGetInstance),
      platform_app, region_str);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(platform_app);
  env->DeleteLocalRef(region_str);
  assert(functions_obj != nullptr);
  obj_ = env->NewGlobalRef(functions_obj);
  env->DeleteLocalRef(functions_obj);
}

FunctionsInternal::~FunctionsInternal() {
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  JNIEnv* env = app_->GetJNIEnv();
  env->DeleteGlobalRef(obj_);
  obj_ = nullptr;
  Terminate(app_);
  app_ = nullptr;

  util::CheckAndClearJniExceptions(env);
}

bool FunctionsInternal::Initialize(App* app) {
  MutexLock init_lock(init_mutex_);
  if (initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    jobject activity = app->activity();
    if (!(firebase_functions::CacheMethodIds(env, activity) &&
          functions_exception::CacheMethodIds(env, activity) &&
          functions_exception_code::CacheMethodIds(env, activity) &&
          functions_exception_code::CacheFieldIds(env, activity) &&
          // Call Initialize on all other Functions internal classes.
          HttpsCallableReferenceInternal::Initialize(app))) {
      return false;
    }
    util::CheckAndClearJniExceptions(env);
  }
  initialize_count_++;
  return true;
}

void FunctionsInternal::Terminate(App* app) {
  MutexLock init_lock(init_mutex_);
  assert(initialize_count_ > 0);
  initialize_count_--;
  if (initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    firebase_functions::ReleaseClass(env);
    functions_exception::ReleaseClass(env);

    // Call Terminate on all other Functions internal classes.
    HttpsCallableReferenceInternal::Terminate(app);

    util::CheckAndClearJniExceptions(env);
  }
}

App* FunctionsInternal::app() const { return app_; }
const char* FunctionsInternal::region() const { return region_.c_str(); }

Error FunctionsInternal::ErrorFromJavaFunctionsException(
    jobject java_exception, std::string* error_message) const {
  JNIEnv* env = app_->GetJNIEnv();
  Error error = kErrorNone;
  if (java_exception != nullptr) {
    // Guarantee that it is a functions exception before getting the code.
    if (env->IsInstanceOf(java_exception, functions_exception::GetClass())) {
      jobject java_code = env->CallObjectMethod(
          java_exception,
          functions_exception::GetMethodId(functions_exception::kGetCode));
      if (java_code) {
        jint code = env->CallIntMethod(java_code,
                                       functions_exception_code::GetMethodId(
                                           functions_exception_code::kOrdinal));
        env->DeleteLocalRef(java_code);
        error = static_cast<Error>(code);
      }
    } else {
      // The exception wasn't a Functions exception, so tag it as unknown.
      error = kErrorUnknown;
    }
    if (error_message != nullptr) {
      *error_message = util::GetMessageFromException(env, java_exception);
    }
    util::CheckAndClearJniExceptions(env);
  }
  return error;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    const char* name) const {
  FIREBASE_ASSERT_RETURN(nullptr, name != nullptr);
  JNIEnv* env = app_->GetJNIEnv();
  jobject name_string = env->NewStringUTF(name);
  jobject callable_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_functions::GetMethodId(firebase_functions::kGetHttpsCallable),
      name_string);
  env->DeleteLocalRef(name_string);
  if (util::LogException(env, kLogLevelError,
                         "Functions::GetHttpsCallable() (name = %s) failed",
                         name)) {
    return nullptr;
  }
  HttpsCallableReferenceInternal* internal = new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), callable_reference_obj);
  env->DeleteLocalRef(callable_reference_obj);
  util::CheckAndClearJniExceptions(env);
  return internal;
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  FIREBASE_ASSERT(origin != nullptr);
  JNIEnv* env = app_->GetJNIEnv();
  jobject origin_string = env->NewStringUTF(origin);
  env->CallVoidMethod(obj_,
                      firebase_functions::GetMethodId(
                          firebase_functions::kUseFunctionsEmulator),
                      origin_string);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(origin_string);
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
