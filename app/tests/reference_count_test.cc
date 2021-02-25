/*
 * Copyright 2019 Google LLC
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

#include "app/src/reference_count.h"

#include "app/src/mutex.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Eq;
using ::testing::IsNull;

using ::firebase::internal::ReferenceCount;
using ::firebase::internal::ReferenceCountedInitializer;
using ::firebase::internal::ReferenceCountLock;

class ReferenceCountTest : public ::testing::Test {
 protected:
  ReferenceCount count_;
};

TEST_F(ReferenceCountTest, Construct) {
  EXPECT_THAT(count_.references(), Eq(0));
}

TEST_F(ReferenceCountTest, AddReference) {
  EXPECT_THAT(count_.AddReference(), Eq(0));
  EXPECT_THAT(count_.references(), Eq(1));
  EXPECT_THAT(count_.AddReference(), Eq(1));
  EXPECT_THAT(count_.references(), Eq(2));
}

TEST_F(ReferenceCountTest, RemoveReference) {
  count_.AddReference();
  count_.AddReference();
  EXPECT_THAT(count_.RemoveReference(), Eq(2));
  EXPECT_THAT(count_.references(), Eq(1));
  EXPECT_THAT(count_.RemoveReference(), Eq(1));
  EXPECT_THAT(count_.references(), Eq(0));
  EXPECT_THAT(count_.RemoveReference(), Eq(0));
  EXPECT_THAT(count_.references(), Eq(0));
}

TEST_F(ReferenceCountTest, RemoveAllReferences) {
  count_.AddReference();
  count_.AddReference();
  EXPECT_THAT(count_.RemoveAllReferences(), Eq(2));
  EXPECT_THAT(count_.references(), Eq(0));
}

class ReferenceCountLockTest : public ::testing::Test {
 protected:
  void SetUp() override { count_.AddReference(); }

 protected:
  ReferenceCount count_;
};

TEST_F(ReferenceCountLockTest, Construct) {
  {
    ReferenceCountLock<ReferenceCount> lock(&count_);
    EXPECT_THAT(lock.references(), Eq(1));
    EXPECT_THAT(count_.references(), Eq(2));
  }
  EXPECT_THAT(count_.references(), Eq(1));
}

TEST_F(ReferenceCountLockTest, AddReference) {
  ReferenceCountLock<ReferenceCount> lock(&count_);
  EXPECT_THAT(lock.references(), Eq(1));
  EXPECT_THAT(lock.AddReference(), Eq(1));
  EXPECT_THAT(lock.references(), Eq(2));
}

TEST_F(ReferenceCountLockTest, RemoveReference) {
  ReferenceCountLock<ReferenceCount> lock(&count_);
  lock.AddReference();
  lock.AddReference();
  EXPECT_THAT(lock.RemoveReference(), Eq(3));
  EXPECT_THAT(lock.references(), Eq(2));
  EXPECT_THAT(lock.RemoveReference(), Eq(2));
  EXPECT_THAT(lock.references(), Eq(1));
  EXPECT_THAT(lock.RemoveReference(), Eq(1));
  EXPECT_THAT(lock.references(), Eq(0));
  EXPECT_THAT(lock.RemoveReference(), Eq(0));
  EXPECT_THAT(lock.references(), Eq(0));
}

TEST_F(ReferenceCountLockTest, RemoveAllReferences) {
  ReferenceCountLock<ReferenceCount> lock(&count_);
  lock.AddReference();
  EXPECT_THAT(lock.references(), Eq(2));
  EXPECT_THAT(lock.RemoveAllReferences(), Eq(2));
  EXPECT_THAT(lock.references(), Eq(0));
  EXPECT_THAT(count_.references(), Eq(0));
}

class ReferenceCountedInitializerTest : public ::testing::Test {
 protected:
  // Object to initialize in Initialize().
  struct Context {
    bool initialize_success;
    int initialized_count;
  };

 protected:
  // Initialize the context object.
  static bool Initialize(Context* context) {
    if (!context->initialize_success) return false;
    context->initialized_count++;
    return true;
  }

  static void Terminate(Context* context) { context->initialized_count--; }
};

TEST_F(ReferenceCountedInitializerTest, ConstructEmpty) {
  ReferenceCountedInitializer<void> initializer;
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(initializer.initialize(), IsNull());
  EXPECT_THAT(initializer.terminate(), IsNull());
  EXPECT_THAT(initializer.context(), IsNull());
  // Use the mutex accessor to instantiate the template.
  firebase::MutexLock lock(initializer.mutex());
}

TEST_F(ReferenceCountedInitializerTest, ConstructWithTerminate) {
  Context context;
  ReferenceCountedInitializer<Context> initializer(Terminate, &context);
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(initializer.initialize(), IsNull());
  EXPECT_THAT(initializer.terminate(), Eq(Terminate));
  EXPECT_THAT(initializer.context(), Eq(&context));
}

TEST_F(ReferenceCountedInitializerTest, ConstructWithInitializeAndTerminate) {
  Context context;
  ReferenceCountedInitializer<Context> initializer(Initialize, Terminate,
                                                   &context);
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(initializer.initialize(), Eq(Initialize));
  EXPECT_THAT(initializer.terminate(), Eq(Terminate));
  EXPECT_THAT(initializer.context(), Eq(&context));
}

TEST_F(ReferenceCountedInitializerTest, SetContext) {
  Context context;
  ReferenceCountedInitializer<Context> initializer(Initialize, Terminate,
                                                   nullptr);
  EXPECT_THAT(initializer.context(), IsNull());
  initializer.set_context(&context);
  EXPECT_THAT(initializer.context(), Eq(&context));
}

TEST_F(ReferenceCountedInitializerTest, AddReferenceNoInit) {
  ReferenceCountedInitializer<Context> initializer(nullptr, nullptr, nullptr);
  EXPECT_THAT(initializer.AddReference(), Eq(0));
  EXPECT_THAT(initializer.references(), Eq(1));
  EXPECT_THAT(initializer.AddReference(), Eq(1));
  EXPECT_THAT(initializer.references(), Eq(2));
}

TEST_F(ReferenceCountedInitializerTest, AddReferenceInlineInit) {
  Context context = {true, 0};
  ReferenceCountedInitializer<Context> initializer;
  EXPECT_THAT(initializer.AddReference(
                  [](Context* state) {
                    state->initialized_count = 12345678;
                    return true;
                  },
                  &context),
              Eq(0));
  EXPECT_THAT(initializer.references(), Eq(1));
  EXPECT_THAT(context.initialized_count, Eq(12345678));
}

TEST_F(ReferenceCountedInitializerTest, AddReferenceSuccessfulInit) {
  Context context = {true, 0};
  ReferenceCountedInitializer<Context> initializer(Initialize, Terminate,
                                                   &context);
  EXPECT_THAT(initializer.AddReference(), Eq(0));
  EXPECT_THAT(initializer.references(), Eq(1));
  EXPECT_THAT(context.initialized_count, Eq(1));
  EXPECT_THAT(initializer.AddReference(), Eq(1));
  EXPECT_THAT(initializer.references(), Eq(2));
  EXPECT_THAT(context.initialized_count, Eq(1));
}

TEST_F(ReferenceCountedInitializerTest, AddReferenceFailedInit) {
  Context context = {false, 0};
  ReferenceCountedInitializer<Context> initializer(Initialize, Terminate,
                                                   &context);
  EXPECT_THAT(initializer.AddReference(), Eq(-1));
  EXPECT_THAT(initializer.references(), Eq(0));
}

TEST_F(ReferenceCountedInitializerTest, RemoveReferenceNoInit) {
  Context context = {true, 3};
  ReferenceCountedInitializer<Context> initializer(nullptr, Terminate,
                                                   &context);
  EXPECT_THAT(initializer.AddReference(), Eq(0));
  EXPECT_THAT(initializer.references(), Eq(1));
  EXPECT_THAT(context.initialized_count, Eq(3));
  EXPECT_THAT(initializer.RemoveReference(), Eq(1));
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(context.initialized_count, Eq(2));
}

TEST_F(ReferenceCountedInitializerTest, RemoveAllReferences) {
  Context context = {true, 3};
  ReferenceCountedInitializer<Context> initializer(nullptr, Terminate,
                                                   &context);
  EXPECT_THAT(initializer.AddReference(), Eq(0));
  EXPECT_THAT(initializer.references(), Eq(1));
  EXPECT_THAT(context.initialized_count, Eq(3));
  EXPECT_THAT(initializer.AddReference(), Eq(1));
  EXPECT_THAT(initializer.references(), Eq(2));
  EXPECT_THAT(initializer.RemoveAllReferences(), Eq(2));
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(context.initialized_count, Eq(2));
}

TEST_F(ReferenceCountedInitializerTest, RemoveAllReferencesWithoutTerminate) {
  Context context = {true, 3};
  ReferenceCountedInitializer<Context> initializer(nullptr, Terminate,
                                                   &context);
  EXPECT_THAT(initializer.AddReference(), Eq(0));
  EXPECT_THAT(initializer.references(), Eq(1));
  EXPECT_THAT(context.initialized_count, Eq(3));
  EXPECT_THAT(initializer.AddReference(), Eq(1));
  EXPECT_THAT(initializer.references(), Eq(2));
  EXPECT_THAT(initializer.RemoveAllReferencesWithoutTerminate(), Eq(2));
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(context.initialized_count, Eq(3));
}

TEST_F(ReferenceCountedInitializerTest, RemoveReferenceSuccessfulInit) {
  Context context = {true, 0};
  ReferenceCountedInitializer<Context> initializer(Initialize, Terminate,
                                                   &context);
  EXPECT_THAT(initializer.AddReference(), Eq(0));
  EXPECT_THAT(initializer.references(), Eq(1));
  EXPECT_THAT(context.initialized_count, Eq(1));
  EXPECT_THAT(initializer.RemoveReference(), Eq(1));
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(context.initialized_count, Eq(0));
  EXPECT_THAT(initializer.RemoveReference(), Eq(0));
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(context.initialized_count, Eq(0));
}

TEST_F(ReferenceCountedInitializerTest, RemoveReferenceFailedInit) {
  Context context = {false, 0};
  ReferenceCountedInitializer<Context> initializer(Initialize, Terminate,
                                                   &context);
  EXPECT_THAT(initializer.AddReference(), Eq(-1));
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(context.initialized_count, Eq(0));
  EXPECT_THAT(initializer.RemoveReference(), Eq(0));
  EXPECT_THAT(initializer.references(), Eq(0));
  EXPECT_THAT(context.initialized_count, Eq(0));
}
