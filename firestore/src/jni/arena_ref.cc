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

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/long.h"
#include "firestore/src/jni/map.h"

namespace firebase {
namespace firestore {
namespace jni {

namespace {

HashMap* gArenaRefHashMap = nullptr;
jclass gLongClass = nullptr;
jmethodID gLongConstructor = nullptr;
std::mutex mutex_;

int64_t GetNextArenaRefKey() {
  static std::atomic<int64_t> next_key(0);
  return next_key.fetch_add(1);
}

}  // namespace

ArenaRef::ArenaRef(Env& env, const Object& object)
    : key_(GetNextArenaRefKey()) {
  Local<Long> key_ref = key_object(env);
  std::lock_guard<std::mutex> lock(mutex_);
  gArenaRefHashMap->Put(env, key_ref, object);
}

ArenaRef::ArenaRef(const ArenaRef& other)
    : key_(other.key_ == kInvalidKey ? kInvalidKey : GetNextArenaRefKey()) {
  if (other.key_ != kInvalidKey) {
    Env env;
    Local<Object> object = other.get(env);
    Local<Long> key_ref = key_object(env);
    std::lock_guard<std::mutex> lock(mutex_);
    gArenaRefHashMap->Put(env, key_ref, object);
  }
}

ArenaRef::ArenaRef(ArenaRef&& other) {
  using std::swap;  // go/using-std-swap
  swap(key_, other.key_);
}

ArenaRef& ArenaRef::operator=(const ArenaRef& other) {
  if (this == &other || (key_ == kInvalidKey && other.key_ == kInvalidKey)) {
    return *this;
  }

  Env env;
  if (other.key_ != kInvalidKey) {
    if (key_ == kInvalidKey) {
      key_ = GetNextArenaRefKey();
    }
    Local<Object> object = other.get(env);
    std::lock_guard<std::mutex> lock(mutex_);
    gArenaRefHashMap->Put(env, key_object(env), object);
  } else {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      gArenaRefHashMap->Remove(env, key_object(env));
    }
    key_ = kInvalidKey;
  }
  return *this;
}

ArenaRef& ArenaRef::operator=(ArenaRef&& other) {
  if (this != &other) {
    std::swap(key_, other.key_);
  }
  return *this;
}

ArenaRef::~ArenaRef() {
  if (key_ != kInvalidKey) {
    Env env;
    ExceptionClearGuard block(env);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      gArenaRefHashMap->Remove(env, key_object(env));
    }
    if (!env.ok()) {
      env.RecordException();
      env.ExceptionClear();
    }
  }
}

Local<Long> ArenaRef::key_object(Env& env) const {
  if (!env.ok()) return {};
  // The reason why using raw JNI calls here rather than the JNI convenience
  // layer is because we need to get rid of JNI convenience layer dependencies
  // in the ArenaRef destructor to avoid racing condition when firestore object
  // gets destroyed.
  jobject key = env.get()->NewObject(gLongClass, gLongConstructor, key_);
  env.RecordException();
  return {env.get(), key};
}

void ArenaRef::Initialize(Env& env) {
  if (gArenaRefHashMap) {
    return;
  }
  Global<HashMap> hash_map(HashMap::Create(env));
  gArenaRefHashMap = new HashMap(hash_map.release());

  auto long_class_local = env.get()->FindClass("java/lang/Long");
  gLongClass = static_cast<jclass>(env.get()->NewGlobalRef(long_class_local));
  env.get()->DeleteLocalRef(long_class_local);
  gLongConstructor = env.get()->GetMethodID(gLongClass, "<init>", "(J)V");

  if (!env.ok()) return;
}

Local<Object> ArenaRef::get(Env& env) const {
  return gArenaRefHashMap->Get(env, key_object(env));
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
