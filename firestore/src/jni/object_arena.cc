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

#include "firestore/src/jni/object_arena.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/long.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Global<ObjectArena> kInstance;
Global<HashMap> kHashMap;

}  // namespace

void ObjectArena::Initialize(Env& env, Loader& loader) {}

ObjectArena& ObjectArena::GetInstance() { return kInstance; }

Local<Object> ObjectArena::Get(Env& env, int64_t key) {
  return kHashMap.Get(env, Long::Create(env, key));
}

int64_t ObjectArena::Put(Env& env, const Object& value) {
  kHashMap.Put(env, Long::Create(env, next_key_), value);
  return next_key_++;
}

void ObjectArena::Remove(Env& env, int64_t key) {
  kHashMap.Remove(env, Long::Create(env, key));
}

int64_t ObjectArena::Dup(Env& env, int64_t key) {
  kHashMap.Put(env, Long::Create(env, next_key_),
               kHashMap.Get(env, Long::Create(env, key)));
  return next_key_++;
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
