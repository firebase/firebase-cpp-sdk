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

jclass kObjectArenaClass = nullptr;
Constructor<Object> kObjectArenaConstructor("()V");
Method<Object> kObjectArenaGet("get", "(J)Ljava/lang/Object;");
Method<jlong> kObjectArenaPut("put", "(Ljava/lang/Object;)J");
Method<void> kObjectArenaRemove("remove", "(J)V");
Method<jlong> kObjectArenaDup("dup", "(J)J");
jobject kObjectArenaInstance = nullptr;

void LoadObjectArenaClass(Loader& loader) {
  // Load the ObjectArena jclass and its method IDs.
  jclass object_arena_class = loader.LoadClass(kObjectArenaClassName);
  loader.LoadAll(kObjectArenaConstructor, kObjectArenaGet, kObjectArenaPut, kObjectArenaRemove, kObjectArenaDup);
  if (!loader.ok()) {
    return;
  }

  // Keep a global reference to the ObjectArena jclass so that the method IDs
  // stored in kObjectArenaConstructor, et al. never get invalidated.
  if (! kObjectArenaClass) {
    jobject object_arena_class_globalref = loader.env()->NewGlobalRef(object_arena_class);
    if (loader.env()->ExceptionCheck()) {
      loader.env()->ExceptionDescribe();
      firebase::LogAssert("Creating global reference to the %s instance failed", kObjectArenaClassName);
      return;
    }
    kObjectArenaClass = (jclass)object_arena_class_globalref;
  }
}

void LoadObjectArenaSingletonInstance(JNIEnv* env) {
  // If the global singleton instance has already been created, then skip the
  // rest of this function, which is responsible for initializing it.
  if (kObjectArenaInstance) {
    return;
  }

  // Create the global singleton instance of ObjectArena.
  jobject object_arena_instance = env->NewObject(kObjectArenaClass, kObjectArenaConstructor.id());
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    firebase::LogAssert("Creating global singleton %s instance failed", kObjectArenaClassName);
    return;
  }

  // Save the global singleton ObjectArena instance to a global variable.
  jobject object_arena_global_instance = env->NewGlobalRef(object_arena_instance);
  env->DeleteLocalRef(object_arena_instance);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    firebase::LogAssert("Creating global reference to the %s instance failed", kObjectArenaClassName);
    return;
  }
  kObjectArenaInstance = object_arena_global_instance;
}

}  // namespace

ArenaRef::ArenaRef(Env& env, jobject object, AdoptExisting) : ArenaRef(env.get(), object) {
}

ArenaRef::ArenaRef(JNIEnv* env, jobject object) : valid_(true), id_(env->CallLongMethod(kObjectArenaInstance, kObjectArenaPut.id(), object)) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    firebase::LogAssert("ArenaRef::ArenaRef() failed");
  }
}

ArenaRef::~ArenaRef() {
  if (!valid_) {
    return;
  }

  Env env;
  ExceptionClearGuard exception_clear_guard(env);
  JNIEnv* jni_env = env.get();

  env.get()->CallVoidMethod(kObjectArenaInstance, kObjectArenaRemove.id(), id_);

  if (jni_env->ExceptionCheck()) {
    jni_env->ExceptionDescribe();
    firebase::LogAssert("ArenaRef::~ArenaRef() failed");
  }
}

Local<Object> ArenaRef::Get(Env& env) const {
  FIREBASE_ASSERT_MESSAGE(valid_, "ArenaRef::Get() must not be called when valid() is false");
  JNIEnv* jni_env = env.get();

  jobject object = jni_env->CallObjectMethod(kObjectArenaInstance, kObjectArenaGet.id(), id_);

  if (jni_env->ExceptionCheck()) {
    jni_env->ExceptionDescribe();
    firebase::LogAssert("ArenaRef::Get() failed");
  }

  return {jni_env, object};
}

void ArenaRef::Initialize(Loader& loader) {
  LoadObjectArenaClass(loader);
  if (loader.ok() && !loader.env()->ExceptionCheck()) {
    LoadObjectArenaSingletonInstance(loader.env());
  }
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
