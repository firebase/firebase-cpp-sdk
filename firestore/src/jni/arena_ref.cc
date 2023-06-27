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

#include <jni.h>

#include <atomic>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {
namespace jni {

namespace {

constexpr char kObjectArenaClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/ObjectArena";

// A convenience class for repeatedly loading Java JNI method IDs from a given
// Java class.
class MethodLoader {
 public:
  MethodLoader(Loader& loader, jclass java_class)
      : loader_(loader), java_class_(java_class) {}

  jmethodID LoadMethodId(const char* name, const char* signature) const {
    if (!loader_.ok()) {
      return {};
    }
    jmethodID method_id =
        loader_.env()->GetStaticMethodID(java_class_, name, signature);
    if (!loader_.ok()) {
      return {};
    }
    return method_id;
  }

 private:
  Loader& loader_;
  jclass java_class_{};
};

// Helper class for calling methods on the Java `ObjectArena` class.
class ObjectArena {
 public:
  // Disable copy and move, since this class has a global singleton instance.
  ObjectArena(const ObjectArena&) = delete;
  ObjectArena(ObjectArena&&) = delete;
  ObjectArena& operator=(const ObjectArena&) = delete;
  ObjectArena& operator=(ObjectArena&&) = delete;

  // Delete the destructor to prevent the global singleton instance from being
  // deleted.
  ~ObjectArena() = delete;

  // Initialize the global singleton instance of this class.
  // This function must be invoked prior to invoking any other static or
  // non-static member functions in this class.
  // This function is NOT thread-safe, and must not be invoked concurrently.
  static void Initialize(Loader& loader) {
    GetOrCreateSingletonInstance().Initialize_(loader);
  }

  // Returns a reference to the global singleton instance of this class.
  // Note that `Initialize()` must be called before this function.
  static const ObjectArena& GetInstance() {
    const ObjectArena& instance = GetOrCreateSingletonInstance();
    FIREBASE_ASSERT_MESSAGE(instance.initialized_,
                            "ObjectArena should be initialized");
    return instance;
  }

  // Calls the Java method ObjectArena.set() with the given arguments.
  void Set(Env& env, jlong id, jobject value) const {
    if (!env.ok()) {
      return;
    }
    env.get()->CallStaticVoidMethod(java_class_, set_, id, value);
  }

  // Calls the Java method ObjectArena.get() with the given argument, returning
  // whatever it returns.
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

  // Calls the Java method ObjectArena.remove() with the given argument.
  void Remove(Env& env, jlong id) const {
    if (!env.ok()) {
      return;
    }
    env.get()->CallStaticVoidMethod(java_class_, remove_, id);
  }

 private:
  // Make the constructor private so that instances cannot be created, except
  // for the global singleton instance.
  ObjectArena() = default;

  static ObjectArena& GetOrCreateSingletonInstance() {
    // Create the global singleton instance on the heap so that the destructor
    // is never invoked. This avoids potential use-after-free issues on
    // application shutdown where some other static object's destructor tries to
    // use the global singleton instance _after_ its destructor has run.
    static auto* instance = new ObjectArena;
    return *instance;
  }

  void Initialize_(Loader& loader) {
    if (initialized_) {
      return;
    }

    jclass java_class = java_class_;
    if (!java_class) {
      java_class = loader.LoadClass(kObjectArenaClassName);
      if (!loader.ok()) {
        return;
      }
      java_class = (jclass)loader.env()->NewGlobalRef(java_class);
      if (!loader.ok()) {
        return;
      }
      java_class_ = java_class;
    }

    MethodLoader method_loader(loader, java_class);
    get_ = method_loader.LoadMethodId("get", "(J)Ljava/lang/Object;");
    set_ = method_loader.LoadMethodId("set", "(JLjava/lang/Object;)V");
    remove_ = method_loader.LoadMethodId("remove", "(J)V");

    initialized_ = loader.ok();
  }

  // Wrap the member variables below in `std::atomic` so that their values set
  // by `Initialize()` are guaranteed to be visible to all threads.
  std::atomic<jclass> java_class_{};
  std::atomic<jmethodID> get_{};
  std::atomic<jmethodID> set_{};
  std::atomic<jmethodID> remove_{};
  std::atomic<bool> initialized_{false};
};

}  // namespace

// Manages an entry in the Java `ObjectArena` map, creating the entry in the
// constructor from a uniquely-generated `jlong` value, and removing the entry
// in the destructor.
class ArenaRef::ObjectArenaEntry final {
 public:
  ObjectArenaEntry(Env& env, jobject object) : id_(GenerateUniqueId()) {
    ObjectArena::GetInstance().Set(env, id_, object);
  }

  // Delete the copy and move constructors and assignment operators to enforce
  // the requirement that every instance has a distinct value for its `id_`
  // member variable.
  ObjectArenaEntry(const ObjectArenaEntry&) = delete;
  ObjectArenaEntry(ObjectArenaEntry&&) = delete;
  ObjectArenaEntry& operator=(const ObjectArenaEntry&) = delete;
  ObjectArenaEntry& operator=(ObjectArenaEntry&&) = delete;

  ~ObjectArenaEntry() {
    Env env;
    ExceptionClearGuard exception_clear_guard(env);
    ObjectArena::GetInstance().Remove(env, id_);

    if (!env.ok()) {
      env.get()->ExceptionDescribe();
      env.get()->ExceptionClear();
      firebase::LogWarning("~ObjectArenaEntry(): ObjectArena::Remove() failed");
    }
  }

  Local<Object> GetReferent(Env& env) const {
    jobject result = ObjectArena::GetInstance().Get(env, id_);
    if (!env.ok()) {
      return {};
    }
    return {env.get(), result};
  }

 private:
  static jlong GenerateUniqueId() {
    // Start the IDs at a large number with an easily-identifiable prefix to
    // make it easier to determine if an instance's ID is "valid" during
    // debugging. Even though this initial value is large, it still leaves room
    // for almost nine quintillion (8,799,130,036,854,775,807) positive values,
    // which should be enough :)
    static std::atomic<jlong> gNextId(424242000000000000L);
    return gNextId.fetch_add(1);
  }

  // Mark `id_` as `const` to reinforce the expectation that instances are
  // immutable and do not support copy/move assignment.
  const jlong id_;
};

ArenaRef::ArenaRef(Env& env, jobject object) { reset(env, object); }

Local<Object> ArenaRef::get(Env& env) const {
  if (!entry_) {
    return {};
  }
  return entry_->GetReferent(env);
}

void ArenaRef::reset(Env& env, const Object& object) {
  reset(env, object.get());
}

void ArenaRef::reset(Env& env, jobject object) {
  entry_ = std::make_shared<ArenaRef::ObjectArenaEntry>(env, object);
}

void ArenaRef::Initialize(Loader& loader) { ObjectArena::Initialize(loader); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
