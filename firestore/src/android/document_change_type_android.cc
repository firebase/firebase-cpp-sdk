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

#include "firestore/src/android/document_change_type_android.h"

#include "../include/firebase/firestore/document_change.h"
#include "app/src/assert.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Method;
using jni::Object;

using Type = DocumentChange::Type;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/DocumentChange$Type";
Method<int32_t> kOrdinal("ordinal", "()I");

}  // namespace

void DocumentChangeTypeInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kOrdinal);
}

Type DocumentChangeTypeInternal::GetType(Env& env) const {
  static constexpr int32_t kMinType = static_cast<int32_t>(Type::kAdded);
  static constexpr int32_t kMaxType = static_cast<int32_t>(Type::kRemoved);

  int32_t ordinal = env.Call(*this, kOrdinal);
  if (ordinal >= kMinType && ordinal <= kMaxType) {
    return static_cast<DocumentChange::Type>(ordinal);
  } else {
    FIREBASE_ASSERT_MESSAGE(false, "Unknown DocumentChange type.");
    return Type::kAdded;
  }
}

}  // namespace firestore
}  // namespace firebase
