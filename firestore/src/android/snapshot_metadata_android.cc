/*
 * Copyright 2020 Google LLC
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

#include "firestore/src/android/snapshot_metadata_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/SnapshotMetadata";
Method<bool> kHasPendingWrites("hasPendingWrites", "()Z");
Method<bool> kIsFromCache("isFromCache", "()Z");

}  // namespace

void SnapshotMetadataInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kHasPendingWrites, kIsFromCache);
}

SnapshotMetadata SnapshotMetadataInternal::ToPublic(Env& env) const {
  bool has_pending_writes = env.Call(*this, kHasPendingWrites);
  bool is_from_cache = env.Call(*this, kIsFromCache);
  return SnapshotMetadata(has_pending_writes, is_from_cache);
}

}  // namespace firestore
}  // namespace firebase
