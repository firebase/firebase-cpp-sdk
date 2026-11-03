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
#include <memory>

#include "app/src/log.h"
#include "firestore/src/jni/declaration.h"
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

StaticMethod<Object> kGet("get", "(J)Ljava/lang/Object;");
StaticMethod<void> kSet("set", "(JLjava/lang/Object;)V");
StaticMethod<void> kRemove("remove", "(J)V");

jlong GenerateUniqueId() {
  // Start the IDs at a large number with an easily-identifiable prefix to
  // make it easier to determine if an instance's ID is "valid" during
  // debugging. Even though this initial value is large, it still leaves room
  // for almost nine quintillion (8,799,130,036,854,775,807) positive values,
  // which should be enough :)
  static std::atomic<jlong> g_next_id(424242000000000000LL);
  return g_next_id.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

// Manages an entry in the Java `ObjectArena` map, removing the entry in the
// destructor.
class ArenaRef::ObjectArenaEntry final {
 public:
  explicit ObjectArenaEntry(jlong id) : id_(id) {}

  // Delete the copy and move constructors and assignment operators to enforce
  // the requirement that every instance has a distinct value for its `id_`
  // member variable.
  ObjectArenaEntry(const ObjectArenaEntry&) = delete;
  ObjectArenaEntry(ObjectArenaEntry&&) = delete;
  ObjectArenaEntry& operator=(const ObjectArenaEntry&) = delete;
  ObjectArenaEntry& operator=(ObjectArenaEntry&&) = delete;

  ~ObjectArenaEntry() {
    if (kRemove.clazz() == nullptr || kRemove.id() == nullptr) {
      return;
    }

    Env env;
    ExceptionClearGuard exception_clear_guard(env);
    env.Call(kRemove, id_);

    if (!env.ok()) {
      if (env.get() != nullptr) {
        env.get()->ExceptionDescribe();
      }
      env.ExceptionClear();
      firebase::LogWarning("~ObjectArenaEntry(): ObjectArena::Remove() failed");
    }
  }

  Local<Object> GetReferent(Env& env) const {
    if (!env.ok() || kGet.clazz() == nullptr || kGet.id() == nullptr) {
      return {};
    }
    Local<Object> result = env.Call(kGet, id_);
    if (!env.ok()) {
      return {};
    }
    return result;
  }

 private:
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
  if (object == nullptr || !env.ok() || kSet.clazz() == nullptr ||
      kSet.id() == nullptr) {
    entry_.reset();
    return;
  }

  const jlong id = GenerateUniqueId();
  entry_ = std::make_shared<ArenaRef::ObjectArenaEntry>(id);
  env.Call(kSet, id, Object(object));
  if (!env.ok()) {
    entry_.reset();
    return;
  }
}

void ArenaRef::Initialize(Loader& loader) {
  if (kGet.clazz() != nullptr && kGet.id() != nullptr &&
      kSet.clazz() != nullptr && kSet.id() != nullptr &&
      kRemove.clazz() != nullptr && kRemove.id() != nullptr) {
    return;
  }
  loader.LoadClass(kObjectArenaClassName, kGet, kSet, kRemove);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
