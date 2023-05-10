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

#include <atomic>
#include <utility>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {

namespace {

constexpr char kObjectArenaClassName[] = PROGUARD_KEEP_CLASS "com/google/firebase/firestore/internal/cpp/ObjectArena";

class ObjectArena {
 public:
  ObjectArena(const ObjectArena&) = delete;
  ObjectArena(ObjectArena&&) = delete;
  ObjectArena& operator=(const ObjectArena&) = delete;
  ObjectArena& operator=(ObjectArena&&) = delete;

  static const ObjectArena& GetInstance() {
    ObjectArena& instance = GetOrCreateSingletonInstance();
    FIREBASE_ASSERT_MESSAGE(instance.initialized_, "ObjectArena initialization failed");
    return instance;
  }

  static void Initialize(Loader& loader) {
    GetOrCreateSingletonInstance().Initialize_(loader);
  }

  void Set(Env& env, jlong id, jobject value) const {
    if (!env.ok()) {
      return;
    }
    env.get()->CallStaticVoidMethod(java_class_, set_, id, value);
  }

  jobject Get(Env& env, jlong id) const {
    if (!env.ok()) {
      return nullptr;
    }
    jobject result = env.get()->CallStaticObjectMethod(java_class_, get_, id);
    if (!env.ok()) {
      return nullptr;
    }
    return result;
  }

  void Remove(Env& env, jlong id) const {
    if (!env.ok()) {
      return;
    }
    env.get()->CallStaticVoidMethod(java_class_, remove_, id);
  }

  void Dup(Env& env, jlong src_id, jlong dest_id) const {
    if (!env.ok()) {
      return;
    }
    env.get()->CallStaticVoidMethod(java_class_, dup_, src_id, dest_id);
  }

 private:
  ObjectArena() = default;

  static ObjectArena& GetOrCreateSingletonInstance() {
    static ObjectArena instance;
    return instance;
  }

  void Initialize_(Loader& loader) {
    if (initialized_) {
      return;
    }

    if (!java_class_) {
      jclass java_class = LoadObjectArenaClass(loader);
      if (!loader.ok()) {
        return;
      }
      java_class_ = java_class;
    }

    get_ = LoadObjectArenaMethodId(loader, "get", "(J)Ljava/lang/Object;");
    set_ = LoadObjectArenaMethodId(loader, "set", "(JLjava/lang/Object;)V");
    remove_ = LoadObjectArenaMethodId(loader, "remove", "(J)V");
    dup_ = LoadObjectArenaMethodId(loader, "dup", "(JJ)V");

    initialized_ = loader.ok();
  }

  static jclass LoadObjectArenaClass(Loader& loader) {
    jclass clazz = loader.LoadClass(kObjectArenaClassName);
    if (!loader.ok()) {
      return {};
    }

    jobject clazz_globalref = loader.env()->NewGlobalRef(clazz);
    if (! loader.ok()) {
      return {};
    }

    return (jclass)clazz_globalref;
  }

  jmethodID LoadObjectArenaMethodId(Loader& loader, const char* name, const char* signature) const {
    if (!loader.ok()) {
      return {};
    }
    jmethodID method_id = loader.env()->GetStaticMethodID(java_class_, name, signature);
    if (!loader.ok()) {
      return {};
    }
    return method_id;
  }

  jclass java_class_{};
  jmethodID get_{};
  jmethodID set_{};
  jmethodID remove_{};
  jmethodID dup_{};
  bool initialized_ = false;
};

jlong GenerateUniqueArenaRefId() {
  static std::atomic<jlong> gNextId(42420000);
  return gNextId.fetch_add(1);
}

}  // namespace

ArenaRef::ArenaRef() : id_(GenerateUniqueArenaRefId()) {
}

ArenaRef::ArenaRef(Env& env, jobject object) : id_(GenerateUniqueArenaRefId()) {
  ObjectArena::GetInstance().Set(env, id_, object);
}

ArenaRef::~ArenaRef() {
  Env env;
  ExceptionClearGuard exception_clear_guard(env);
  ObjectArena::GetInstance().Remove(env, id_);

  if (!env.ok()) {
    env.get()->ExceptionDescribe();
    env.get()->ExceptionClear();
    firebase::LogWarning("ArenaRef::~ArenaRef() failed");
  }
}

ArenaRef::ArenaRef(const ArenaRef& other) : id_(GenerateUniqueArenaRefId()) {
  Env env;
  ObjectArena::GetInstance().Dup(env, other.id_, id_);
}

ArenaRef::ArenaRef(ArenaRef&& other) noexcept : id_(std::exchange(other.id_, GenerateUniqueArenaRefId())) {
}

ArenaRef& ArenaRef::operator=(const ArenaRef& other) {
  if (&other == this) {
    return *this;
  }

  Env env;
  ObjectArena::GetInstance().Dup(env, other.id_, id_);

  return *this;
}

ArenaRef& ArenaRef::operator=(ArenaRef&& other) noexcept {
  std::swap(id_, other.id_);
  return *this;
}

Local<Object> ArenaRef::get(Env& env) const {
  jobject result = ObjectArena::GetInstance().Get(env, id_);
  if (! env.ok()) {
    return {};
  }
  return {env.get(), result};
}

void ArenaRef::reset(Env& env, const Object& object) const {
  ObjectArena::GetInstance().Set(env, id_, object.get());
}

void ArenaRef::Initialize(Loader& loader) {
  ObjectArena::Initialize(loader);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
