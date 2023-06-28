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

#include "firestore/src/android/wrapper.h"

#include <iterator>

#include "app/src/assert.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Object;

}  // namespace

Wrapper::Wrapper(FirestoreInternal* firestore, const Object& obj)
    : firestore_(firestore), obj_(obj) {
  FIREBASE_ASSERT(obj);
}

Wrapper::Wrapper() {
  Firestore* firestore = Firestore::GetInstance();
  FIREBASE_ASSERT(firestore != nullptr);
  firestore_ = firestore->internal_;
  FIREBASE_ASSERT(firestore_ != nullptr);
}

Wrapper::Wrapper(Wrapper* rhs) : Wrapper() {
  if (rhs) {
    firestore_ = rhs->firestore_;
    FIREBASE_ASSERT(firestore_ != nullptr);
    obj_ = rhs->obj_;
  }
}

Wrapper::~Wrapper() = default;

jni::Env Wrapper::GetEnv() const { return firestore_->GetEnv(); }

Object Wrapper::ToJava(const FieldValue& value) {
  return FieldValueInternal::ToJava(value);
}

}  // namespace firestore
}  // namespace firebase
