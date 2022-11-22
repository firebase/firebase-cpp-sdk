/*
 * Copyright 2022 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_

#include <utility>

#include "app/src/assert.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/object_arena.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * An RAII wrapper that uses ObjectArena to manage the reference. It
 * automatically deletes the JNI local reference when it goes out of scope.
 * Copies and moves are handled by creating additional references as required.
 */
class ArenaRef {
 public:
  ArenaRef() = default;

  explicit ArenaRef(const Object& ob) {
    Env env(GetEnv());
    id_ = assignState(ObjectArena::GetInstance().Put(env, ob));
  }

  ArenaRef(const ArenaRef& other) {
    if (other.id_.first) {
      Env env(GetEnv());
      id_ = assignState(ObjectArena::GetInstance().Dup(env, other.id_.second));
    }
  }

  ArenaRef& operator=(const ArenaRef& other) {
    if (id_ != other.id_) {
      Env env(GetEnv());
      if (id_.first) {
        ObjectArena::GetInstance().Remove(env, id_.second);
      }
      reset();
      if (other.id_.first) {
        id_ =
            assignState(ObjectArena::GetInstance().Dup(env, other.id_.second));
      }
    }
    return *this;
  }

  ArenaRef(ArenaRef&& other) noexcept : ArenaRef(other.release()) {}

  ArenaRef& operator=(ArenaRef&& other) noexcept {
    if (id_.second != other.id_.second) {
      Env env(GetEnv());
      if (id_.first) {
        ObjectArena::GetInstance().Remove(env, id_.second);
      }
      id_ = assignState(other.release());
    }
    return *this;
  }

  ~ArenaRef() {
    if (id_.first) {
      Env env(GetEnv());
      ObjectArena::GetInstance().Remove(env, id_.second);
    }
  }

  Local<Object> get() const {
    Env env(GetEnv());
    return ObjectArena::GetInstance().Get(env, id_.second);
  }

  void clear() {
    if (id_.first) {
      Env env(GetEnv());
      ObjectArena::GetInstance().Remove(env, id_.second);
      reset();
    }
  }

 private:
  explicit ArenaRef(int64_t id) { assignState(id); }

  int64_t release() {
    FIREBASE_DEV_ASSERT(id_.first);
    int64_t id = id_.second;
    reset();
    return id;
  }

  std::pair<bool, int64_t> assignState(int64_t id) { id_ = {true, id}; }

  void reset() { id_ = {false, -1}; }

  // A pair of variables which indicate the state of ArenaRef. If id_.first set
  // to
  std::pair<bool, int64_t> id_{false, -1};
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
