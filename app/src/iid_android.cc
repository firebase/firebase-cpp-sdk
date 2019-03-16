/*
 * Copyright 2017 Google LLC
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

#include "app/src/iid.h"
#include "app/src/mutex.h"
#include "app/src/util_android.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace {

int g_instance_count = 0;
::firebase::Mutex g_instance_mutex;  // NOLINT

bool IsFirstCreator() {
  MutexLock lock(g_instance_mutex);
  g_instance_count++;
  return g_instance_count == 1;
}

bool IsLastDestroyer() {
  MutexLock lock(g_instance_mutex);
  g_instance_count--;
  return g_instance_count == 0;
}

// clang-format off
#define IID_METHODS(X)                              \
  X(GetInstance, "getInstance",                     \
    "(Lcom/google/firebase/FirebaseApp;)"           \
    "Lcom/google/firebase/iid/FirebaseInstanceId;", \
    util::kMethodTypeStatic),                       \
  X(GetToken, "getToken", "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(iid, IID_METHODS)

METHOD_LOOKUP_DEFINITION(iid,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/iid/FirebaseInstanceId",
                         IID_METHODS)
}  // namespace

namespace internal {

InstanceId::InstanceId(const App& app) : app_(app) {
  JNIEnv* env = app_.GetJNIEnv();
  if (IsFirstCreator()) {
    util::Initialize(env, app_.activity());

    FIREBASE_ASSERT_MESSAGE(iid::CacheMethodIds(env, app_.activity()),
                            "Failed to cache Java IID classes.");
  }

  jobject iid = env->CallStaticObjectMethod(iid::GetClass(),
                                            iid::GetMethodId(iid::kGetInstance),
                                            static_cast<jobject>(app_.data_));
  iid_ = env->NewGlobalRef(iid);
  env->DeleteLocalRef(iid);
}

InstanceId::~InstanceId() {
  JNIEnv* env = app_.GetJNIEnv();
  env->DeleteGlobalRef(iid_);
  iid_ = nullptr;

  if (IsLastDestroyer()) {
    util::Terminate(env);
    iid::ReleaseClass(env);
  }
}

std::string InstanceId::GetMasterToken() const {
  JNIEnv* env = app_.GetJNIEnv();
  jobject token = env->CallObjectMethod(iid_, iid::GetMethodId(iid::kGetToken));
  return util::JniStringToString(env, token);
}

}  // namespace internal
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
