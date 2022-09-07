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
#include "firestore/src/android/transaction_options_builder_android.h"

#include "firestore/src/jni/env.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;

using TransactionOptionsTestAndroid = FirestoreIntegrationTest;

TEST_F(TransactionOptionsTestAndroid, DefaultTransactionOptions) {
  Env env;
  Local<TransactionOptionsBuilderInternal> builder =
      TransactionOptionsBuilderInternal::Create(env);

  Local<TransactionOptionsInternal> options = builder.Build(env);

  EXPECT_EQ(options.GetMaxAttempts(env), 5);
}

TEST_F(TransactionOptionsTestAndroid, SetMaxAttemptsReturnsSameInstance) {
  Env env;
  Local<TransactionOptionsBuilderInternal> builder =
      TransactionOptionsBuilderInternal::Create(env);

  Local<TransactionOptionsBuilderInternal> retval =
      builder.SetMaxAttempts(env, 42);

  EXPECT_TRUE(env.IsSameObject(builder, retval));
}

TEST_F(TransactionOptionsTestAndroid, SetMaxAttempts) {
  Env env;
  Local<TransactionOptionsBuilderInternal> builder =
      TransactionOptionsBuilderInternal::Create(env);

  builder.SetMaxAttempts(env, 42);

  EXPECT_EQ(builder.Build(env).GetMaxAttempts(env), 42);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
