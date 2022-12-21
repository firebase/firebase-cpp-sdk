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

#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

}  // namespace

ObjectArena::ObjectArena(Env& env) {
  hash_map_class_ = env.get()->FindClass("java/util/HashMap");
  hash_map_ctor_ = env.get()->GetMethodID(hash_map_class_, "<init>", "()V");
  hash_map_ = env.get()->NewGlobalRef(
          env.get()->NewObject(hash_map_class_, hash_map_ctor_));

  hash_map_get_ = env.get()->GetMethodID(hash_map_class_, "get",
                                         "(Ljava/lang/Object;)Ljava/lang/Object;");
  hash_map_put_ = env.get()->GetMethodID(hash_map_class_, "put",
                                         "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  hash_map_remove_ = env.get()->GetMethodID(hash_map_class_, "remove",
                                         "(Ljava/lang/Object;)Ljava/lang/Object;");

  long_class_ = env.get()->FindClass("java/lang/Long");
  long_ctor_ = env.get()->GetMethodID(long_class_, "<init>", "(J)V");
}

ObjectArena& ObjectArena::GetInstance(Env& env) {
  static auto* instance = new ObjectArena(env);
  return *instance;
}

jobject ObjectArena::Get(Env& env, int64_t key) {
  jobject long_key = MakeKey(env, key);
  jobject result = env.get()->CallObjectMethod(hash_map_, hash_map_get_, long_key);
  SIMPLE_HARD_ASSERT(env.ok(), "Get() fails");
  return result;
}

int64_t ObjectArena::Put(Env& env, jobject value) {
  SIMPLE_HARD_ASSERT(reinterpret_cast<jobject>(0x1) != value, "value is null");
  SIMPLE_HARD_ASSERT(reinterpret_cast<jobject>(0x1) != hash_map_, "hash_map_ is null");
  env.RecordException();
  env.get()->CallObjectMethod(hash_map_, hash_map_put_, next_key_, value);
  env.RecordException();
  SIMPLE_HARD_ASSERT(env.ok(), "Put() fails");
  return next_key_++;
}

void ObjectArena::Remove(Env& env, int64_t key) {
  jobject long_key = MakeKey(env, key);
  env.get()->CallObjectMethod(hash_map_, hash_map_remove_, long_key);
  SIMPLE_HARD_ASSERT(env.ok(), "Remove() fails");
}

int64_t ObjectArena::Dup(Env& env, int64_t key) {
  jobject long_key = MakeKey(env, key);
  jobject old_key = env.get()->CallObjectMethod(hash_map_, hash_map_get_, long_key);
  env.get()->CallObjectMethod(hash_map_, hash_map_put_,
                              MakeKey(env, next_key_),
                              old_key);
  SIMPLE_HARD_ASSERT(env.ok(), "Dup() fails");
  return next_key_++;
}
jobject ObjectArena::MakeKey(Env& env, int64_t key) {
  jobject result = env.get()->NewObject(long_class_, long_ctor_, key);
  SIMPLE_HARD_ASSERT(env.ok(), "MakeKey() fails");
  return result;
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
