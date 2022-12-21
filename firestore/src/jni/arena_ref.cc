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
#include <atomic>

#include "firestore/src/jni/arena_ref.h"

#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/long.h"
#include "firestore/src/jni/map.h"

namespace firebase {
namespace firestore {
namespace jni {

namespace {

HashMap* gArenaRefHashMap = nullptr;

int64_t GetNextArenaRefKey() {
  static std::atomic<int64_t> next_key(1);
  return next_key.fetch_add(1);
}

}

ArenaRef::ArenaRef(Env& env, const Object& object) : key_(GetNextArenaRefKey()) {
  gArenaRefHashMap->Put(env, key_object(env), object);
}

ArenaRef::~ArenaRef() {
  if (key_ != 0) {
    Env env;
    gArenaRefHashMap->Remove(env, key_object(env));
  }
}

Local<Long> ArenaRef::key_object(Env& env) const {
  return Long::Create(env, key_);
}

void ArenaRef::Initialize(Env& env) {
  if (gArenaRefHashMap) {
    return;
  }
  Global<HashMap> hash_map(HashMap::Create(env));
  jobject hash_map_jobject = hash_map.release();
  gArenaRefHashMap = new HashMap(hash_map_jobject);
}

Local<Object> ArenaRef::get(Env& env) const {
  return gArenaRefHashMap->Get(env, key_object(env));
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
