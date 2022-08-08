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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_OPTIONS_BUILDER_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_OPTIONS_BUILDER_ANDROID_H_

#include "firestore/src/android/transaction_options_android.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {

class TransactionOptionsBuilderInternal : public jni::Object {
 public:
  using jni::Object::Object;

  static void Initialize(jni::Loader& loader);

  static jni::Local<TransactionOptionsBuilderInternal> Create(jni::Env&);

  jni::Local<TransactionOptionsBuilderInternal> SetMaxAttempts(
      jni::Env&, int32_t max_attempts) const;

  jni::Local<TransactionOptionsInternal> Build(jni::Env&) const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_OPTIONS_BUILDER_ANDROID_H_
