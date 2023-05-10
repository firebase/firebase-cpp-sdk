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
#include "firestore/src/jni/arena_ref.h"

#include <utility>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {

namespace {

constexpr char kObjectArenaClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/internal/cpp/ObjectArena";

struct ObjectArenaMethodIds {
  jmethodID constructor{};
  jmethodID get{};
  jmethodID put{};
  jmethodID update{};
  jmethodID remove{};
  jmethodID dup1{};
  jmethodID dup2{};
  bool initialized = false;
};

jclass kObjectArenaClass;
ObjectArenaMethodIds kObjectArenaMethodIds;
jobject kObjectArenaSingletonInstance;

void LoadObjectArenaClass(Loader& loader) {
  JNIEnv* env = loader.env();
  if (kObjectArenaClass || !loader.ok() || env->ExceptionCheck()) {
    return;
  }

  jclass object_arena_class_loaderref = loader.LoadClass(kObjectArenaClassName);
  if (env->ExceptionCheck()) {
    return;
  }

  jobject object_arena_class_globalref = env->NewGlobalRef(object_arena_class_loaderref);
  if (env->ExceptionCheck()) {
    return;
  }

  kObjectArenaClass = (jclass)object_arena_class_globalref;
}

jmethodID LoadObjectArenaMethodId(JNIEnv* env, const char* name, const char* signature) {
  if (!kObjectArenaClass || env->ExceptionCheck()) {
    return {};
  }
  return env->GetMethodID(kObjectArenaClass, name, signature);
}

void LoadObjectArenaMethodIds(JNIEnv* env) {
  if (!kObjectArenaClass || kObjectArenaMethodIds.initialized) {
    return;
  }
  kObjectArenaMethodIds.constructor = LoadObjectArenaMethodId(env, "<init>", "()V");
  kObjectArenaMethodIds.get = LoadObjectArenaMethodId(env, "get", "(J)Ljava/lang/Object;");
  kObjectArenaMethodIds.put = LoadObjectArenaMethodId(env, "put", "(Ljava/lang/Object;)J");
  kObjectArenaMethodIds.update = LoadObjectArenaMethodId(env, "update", "(JLjava/lang/Object;)V");
  kObjectArenaMethodIds.remove = LoadObjectArenaMethodId(env, "remove", "(J)V");
  kObjectArenaMethodIds.dup1 = LoadObjectArenaMethodId(env, "dup", "(J)J");
  kObjectArenaMethodIds.dup2 = LoadObjectArenaMethodId(env, "dup", "(JJ)V");
  if (!env->ExceptionCheck()) {
    kObjectArenaMethodIds.initialized = true;
  }
}

void LoadObjectArenaSingletonInstance(JNIEnv* env) {
  if (kObjectArenaSingletonInstance || !kObjectArenaMethodIds.initialized || env->ExceptionCheck()) {
    return;
  }

  // Create the global singleton instance of ObjectArena.
  jobject object_arena_instance_localref = env->NewObject(kObjectArenaClass, kObjectArenaMethodIds.constructor);
  if (env->ExceptionCheck()) {
    return;
  }

  // Save the global singleton ObjectArena instance to a global variable.
  jobject object_arena_instance_globalref = env->NewGlobalRef(object_arena_instance_localref);
  env->DeleteLocalRef(object_arena_instance_localref);
  if (env->ExceptionCheck()) {
    return;
  }
  kObjectArenaSingletonInstance = object_arena_instance_globalref;
}

}  // namespace

ArenaRef::ArenaRef(Env& env, jobject object) {
  if (!env.ok()) {
    valid_ = false;
  } else {
    id_ = env.get()->CallLongMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.put, object);
    valid_ = env.ok();
  }
}

ArenaRef::~ArenaRef() {
  if (!valid_) {
    return;
  }

  Env env;
  ExceptionClearGuard exception_clear_guard(env);
  JNIEnv* jni_env = env.get();

  jni_env->CallVoidMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.remove, id_);

  if (jni_env->ExceptionCheck()) {
    jni_env->ExceptionDescribe();
    jni_env->ExceptionClear();
    firebase::LogWarning("ArenaRef::~ArenaRef() failed");
  }
}

ArenaRef::ArenaRef(const ArenaRef& other) {
  Env env;
  if (! env.ok()) {
    valid_ = false;
    return;
  }

  valid_ = other.valid_;
  if (valid_) {
    id_ = env.get()->CallLongMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.dup1, other.id_);
    valid_ = env.ok();
  }
}

ArenaRef::ArenaRef(ArenaRef&& other) noexcept : valid_(std::exchange(other.valid_, false)), id_(other.id_) {
}

ArenaRef& ArenaRef::operator=(const ArenaRef& other) {
  if (&other == this) {
    return *this;
  }

  Env env;
  if (!env.ok()) {
    return *this;
  }
  JNIEnv* jni_env = env.get();

  if (!other.valid_) {
    if (valid_) {
      jni_env->CallVoidMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.remove, id_);
      if (jni_env->ExceptionCheck()) {
        jni_env->ExceptionDescribe();
        firebase::LogAssert("ArenaRef::operator=(const ArenaRef&) ObjectArena.remove() failed");
      }
    }
    valid_ = false;
  } else if (valid_) {
    jni_env->CallVoidMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.dup2, other.id_, id_);
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      firebase::LogAssert("ArenaRef::operator=(const ArenaRef&) ObjectArena.dup(long, long) failed");
    }
  } else {
    id_ = jni_env->CallLongMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.dup1, other.id_);
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      firebase::LogAssert("ArenaRef::operator=(const ArenaRef&) ObjectArena.dup(long) failed");
    }
    valid_ = true;
  }

  return *this;
}

ArenaRef& ArenaRef::operator=(ArenaRef&& other) noexcept {
  // go/using-std-swap
  using std::swap;
  swap(valid_, other.valid_);
  swap(id_, other.id_);
  return *this;
}

Local<Object> ArenaRef::get(Env& env) const {
  if (! env.ok() || !valid_) {
    return {};
  }
  JNIEnv* jni_env = env.get();

  jobject object = jni_env->CallObjectMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.get, id_);

  return {jni_env, object};
}

void ArenaRef::reset(Env& env, const Object& object) {
  if (! env.ok()) {
    return;
  }
  JNIEnv* jni_env = env.get();

  if (valid_) {
    jni_env->CallObjectMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.update, id_, object.get());
  } else {
    id_ = jni_env->CallLongMethod(kObjectArenaSingletonInstance, kObjectArenaMethodIds.put, object.get());
    valid_ = true;
  }
}

void ArenaRef::Initialize(Loader& loader) {
  if (!loader.ok()) {
    return;
  }

  LoadObjectArenaClass(loader);
  JNIEnv* env = loader.env();
  LoadObjectArenaMethodIds(env);
  LoadObjectArenaSingletonInstance(env);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    firebase::LogAssert("ArenaRef::Initialize() failed");
  }
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
