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
    Env env;
    int64_t id = ObjectArena::GetInstance(env).Put(env, ob.get());
    if (env.ok()) {
      assignState(id);
    }
  }

  explicit ArenaRef(const ArenaRef& other) {
    if (other.valid_) {
      Env env;
      int64_t id = ObjectArena::GetInstance(env).Dup(env, other.id_);
      if (env.ok()) {
        assignState(id);
      }
    }
  }

  ArenaRef& operator=(const ArenaRef& other) {
    Env env;
    clear(env);
    if (other.valid_ && id_ != other.id_) {
      if (other.valid_) {
        int64_t id = ObjectArena::GetInstance(env).Dup(env, other.id_);
        if (env.ok()) {
          assignState(id);
        }
      }
    }
    return *this;
  }

  explicit ArenaRef(ArenaRef&& other) {
    if (other.valid_) {
      assignState(other.release());
    }
  }

  ArenaRef& operator=(ArenaRef&& other) {
    Env env(GetEnv());
    clear(env);
    if (other.valid_ && id_ != other.id_) {
      if (other.valid_) {
        assignState(other.release());
      }
    }
    return *this;
  }

  ~ArenaRef() {
    if (valid_) {
      Env env;
      ObjectArena::GetInstance(env).Remove(env, id_);
    }
  }

  bool has_value() const { return valid_; }

  Local<Object> get(Env& env) const {
    FIREBASE_DEV_ASSERT(valid_);
    return env.MakeResult<Object>(ObjectArena::GetInstance(env).Get(env, id_));
  }

  void clear(Env& env) {
    if (valid_) {
      ObjectArena::GetInstance(env).Remove(env, id_);
      reset();
    }
  }

  bool is_valid() { return valid_ && id_ >= 0; }

 private:
  int64_t release() {
    FIREBASE_DEV_ASSERT(valid_);
    int64_t id = id_;
    reset();
    return id;
  }

  void assignState(int64_t id) {
    valid_ = true;
    id_ = id;
  }

  void reset() {
    valid_ = false;
    id_ = -1;
  }

  // valid_ indicate the state of ArenaRef
  bool valid_ = false;
  int64_t id_ = -1;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
