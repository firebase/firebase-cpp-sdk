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

#include "firestore/src/android/transaction_options_android.h"

#include "firestore/src/jni/declaration.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Loader;
using jni::Method;

constexpr char kTransactionOptionsClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/TransactionOptions";
Method<int32_t> kGetMaxAttempts("getMaxAttempts", "()I");

}  // namespace

void TransactionOptionsInternal::Initialize(Loader& loader) {
  loader.LoadClass(kTransactionOptionsClass, kGetMaxAttempts);
}

int32_t TransactionOptionsInternal::GetMaxAttempts(Env& env) const {
  return env.Call(*this, kGetMaxAttempts);
}

}  // namespace firestore
}  // namespace firebase
