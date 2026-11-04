// Copyright 2023 Google LLC

#include "firestore/src/jni/arena_ref.h"

#include <jni.h>

#include <atomic>
#include <memory>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase::firestore::jni {
namespace {

constexpr char kObjectArenaClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/ObjectArena";

class ObjectArena {
 public:
  static void Initialize(Loader& loader) {
    GetOrCreateSingleton().Initialize_(loader);
  }
  static const ObjectArena& GetInstance() {
    const ObjectArena& instance = GetOrCreateSingleton();
    FIREBASE_ASSERT_MESSAGE(instance.initialized_,
                            "ObjectArena should be initialized");
    return instance;
  }

  void Set(Env& env, jlong id, jobject value) const {
    if (env.ok() && initialized_) {
      env.get()->CallStaticVoidMethod(java_class_, set_, id, value);
    }
  }
  Local<Object> Get(Env& env, jlong id) const {
    if (!env.ok() || !initialized_) return {};
    jobject result = env.get()->CallStaticObjectMethod(java_class_, get_, id);
    return env.ok() ? Local<Object>(env.get(), result) : Local<Object>();
  }
  void Remove(Env& env, jlong id) const {
    if (env.ok() && initialized_) {
      env.get()->CallStaticVoidMethod(java_class_, remove_, id);
    }
  }

 private:
  static ObjectArena& GetOrCreateSingleton() {
    static auto* instance = new ObjectArena;
    return *instance;
  }

  void Initialize_(Loader& loader) {
    if (initialized_) return;
    jclass java_class = java_class_;
    if (!java_class) {
      java_class = loader.LoadClass(kObjectArenaClassName);
      if (!loader.ok() || !java_class) return;
      java_class = static_cast<jclass>(loader.env()->NewGlobalRef(java_class));
      if (!loader.ok() || !java_class) return;
      java_class_ = java_class;
    }
    auto load_method = [&](const char* name, const char* sig) -> jmethodID {
      return loader.ok() ? loader.env()->GetStaticMethodID(java_class, name, sig)
                         : nullptr;
    };
    jmethodID get = load_method("get", "(J)Ljava/lang/Object;");
    jmethodID set = load_method("set", "(JLjava/lang/Object;)V");
    jmethodID remove = load_method("remove", "(J)V");
    if (!loader.ok() || !get || !set || !remove) return;
    get_ = get;
    set_ = set;
    remove_ = remove;
    initialized_ = true;
  }

  std::atomic<jclass> java_class_{};
  std::atomic<jmethodID> get_{};
  std::atomic<jmethodID> set_{};
  std::atomic<jmethodID> remove_{};
  std::atomic<bool> initialized_{false};
};

jlong GenerateUniqueId() {
  static std::atomic<jlong> g_next_id(424242000000000000LL);
  return g_next_id.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

class ArenaRef::ObjectArenaEntry final {
 public:
  explicit ObjectArenaEntry(jlong id) : id_(id) {}
  ~ObjectArenaEntry() {
    Env env;
    JNIEnv* jni_env = env.get();
    if (!jni_env) return;
    jthrowable pending = jni_env->ExceptionOccurred();
    if (pending) jni_env->ExceptionClear();
    {
      ExceptionClearGuard guard(env);
      ObjectArena::GetInstance().Remove(env, id_);
      if (!env.ok()) {
        jni_env->ExceptionDescribe();
        env.ExceptionClear();
        firebase::LogWarning(
            "~ObjectArenaEntry(): ObjectArena::Remove() failed");
      }
    }
    if (pending) {
      jni_env->Throw(pending);
      jni_env->DeleteLocalRef(pending);
    }
  }

  Local<Object> GetReferent(Env& env) const {
    return ObjectArena::GetInstance().Get(env, id_);
  }

 private:
  const jlong id_;
};

ArenaRef::ArenaRef(Env& env, jobject object) { reset(env, object); }
Local<Object> ArenaRef::get(Env& env) const {
  return entry_ ? entry_->GetReferent(env) : Local<Object>();
}
void ArenaRef::reset(Env& env, const Object& object) {
  reset(env, object.get());
}
void ArenaRef::reset(Env& env, jobject object) {
  entry_.reset();
  if (!object || !env.ok()) return;
  const jlong id = GenerateUniqueId();
  ObjectArena::GetInstance().Set(env, id, object);
  if (env.ok()) entry_ = std::make_shared<ArenaRef::ObjectArenaEntry>(id);
}
void ArenaRef::Initialize(Loader& loader) { ObjectArena::Initialize(loader); }

}  // namespace firebase::firestore::jni
