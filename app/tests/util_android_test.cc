/*
 * Copyright 2017 Google LLC
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

#include "app/src/util_android.h"

#include <pthread.h>
#include <utility>

#include "app/src/include/firebase/variant.h"
#include "app/src/semaphore.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/run_all_tests.h"

namespace firebase {
namespace util {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Ne;

TEST(UtilAndroidTest, TestInitializeAndTerminate) {
  // Initialize firebase util, including caching class/methods and dealing with
  // embedded jar.
  JNIEnv *env = firebase::testing::cppsdk::GetTestJniEnv();
  EXPECT_NE(nullptr, env);
  jobject activity_object = firebase::testing::cppsdk::GetTestActivity();
  EXPECT_NE(nullptr, activity_object);
  EXPECT_TRUE(Initialize(env, activity_object));

  Terminate(env);
}

TEST(JniUtilities, LocalToGlobalReference) {
  JNIEnv *env = firebase::testing::cppsdk::GetTestJniEnv();
  jobject local_java_string = env->NewStringUTF("a string");
  jobject global_java_string = LocalToGlobalReference(env, local_java_string);
  EXPECT_NE(nullptr, global_java_string);
  env->DeleteGlobalRef(global_java_string);

  EXPECT_EQ(nullptr, LocalToGlobalReference(env, nullptr));
}

// Test execution on the main and background Java threads.
class JavaThreadContextTest : public ::testing::Test {
 protected:
  class ThreadContext {
   public:
    explicit ThreadContext(JavaThreadContext *java_thread_context = nullptr)
        : started_(1),
          complete_(1),
          block_store_(1),
          canceled_(false),
          cancel_store_called_(false),
          java_thread_context_(java_thread_context) {
      thread_id_ = pthread_self();
      started_.Wait();
      complete_.Wait();
      block_store_.Wait();
    }

    // Wait for the thread to start.
    void WaitForStart() { started_.Wait(); }

    // Wait for the thread to complete.
    void WaitForCompletion() { complete_.Wait(); }

    // Continue Store() execution (if it's blocked).
    void Continue() { block_store_.Post(); }

    // Get the thread ID.
    pthread_t thread_id() const { return thread_id_; }

    // Get whether the thread was canceled.
    bool canceled() const { return canceled_; }

    // Get whether CancelStore was called.
    bool cancel_store_called() const { return cancel_store_called_; }

    // Store the current thread ID and signal thread completion.
    static void Store(void *data) {
      static_cast<ThreadContext *>(data)->Store(false);
    }

    // Wait for Continue() to be called then store the current thread ID
    // if the context wasn't cancelled and signal thread completion.
    static void WaitAndStore(void *data) {
      static_cast<ThreadContext *>(data)->Store(true);
    }

    // Cancel the store operation.
    static void CancelStore(void *data) {
      static_cast<ThreadContext *>(data)->cancel_store_called_ = true;
    }

   private:
    // Store the current thread ID if the object wasn't canceled and
    // signal thread completion.
    void Store(bool wait) {
      if (wait) {
        if (java_thread_context_) {
          // Release the execution lock so the thread can be canceled.
          java_thread_context_->ReleaseExecuteCancelLock();
        }
        // Signal that the thread has started.
        started_.Post();
        // Wait for Continue().
        block_store_.Wait();
        if (java_thread_context_) {
          // If this method returns false, the thread is canceled.
          canceled_ = !java_thread_context_->AcquireExecuteCancelLock();
        }
      } else {
        started_.Post();
      }
      if (!canceled_) thread_id_ = pthread_self();
      complete_.Post();
    }

   private:
    // ID of the thread.
    pthread_t thread_id_;
    // Signalled when the thread starts.
    Semaphore started_;
    // Signalled when a thread is complete.
    Semaphore complete_;
    // Used to block execution of Store().
    Semaphore block_store_;
    // Whether the Store() operation was canceled.
    bool canceled_;
    // Whether CancelStore() was called.
    bool cancel_store_called_;
    JavaThreadContext *java_thread_context_;
  };

  void SetUp() override {
    env_ = firebase::testing::cppsdk::GetTestJniEnv();
    ASSERT_TRUE(env_ != nullptr);
    activity_ = firebase::testing::cppsdk::GetTestActivity();
    ASSERT_TRUE(activity_ != nullptr);
    ASSERT_TRUE(Initialize(env_, activity_));
  }

  void TearDown() override {
    ASSERT_TRUE(env_ != nullptr);
    Terminate(env_);
  }

  JNIEnv *env_;
  jobject activity_;
};

TEST_F(JavaThreadContextTest, RunOnMainThread) {
  ThreadContext thread_context(nullptr);
  pthread_t main_thread_id = thread_context.thread_id();
  RunOnMainThread(env_, activity_, ThreadContext::Store, &thread_context);
  thread_context.WaitForCompletion();
  EXPECT_THAT(thread_context.thread_id(), Ne(main_thread_id));
}

TEST_F(JavaThreadContextTest, RunOnMainThreadAndCancel) {
  JavaThreadContext java_thread_context(env_);
  ThreadContext thread_context(&java_thread_context);
  pthread_t main_thread_id = thread_context.thread_id();
  RunOnMainThread(env_, activity_, ThreadContext::WaitAndStore, &thread_context,
                  ThreadContext::CancelStore, &java_thread_context);
  thread_context.WaitForStart();
  java_thread_context.Cancel();
  thread_context.Continue();
  thread_context.WaitForCompletion();
  EXPECT_THAT(thread_context.thread_id(), Eq(main_thread_id));
  EXPECT_TRUE(thread_context.canceled());
  EXPECT_TRUE(thread_context.cancel_store_called());
}

TEST_F(JavaThreadContextTest, RunOnBackgroundThread) {
  ThreadContext thread_context(nullptr);
  pthread_t main_thread_id = thread_context.thread_id();
  RunOnBackgroundThread(env_, ThreadContext::Store, &thread_context);
  thread_context.WaitForCompletion();
  EXPECT_THAT(thread_context.thread_id(), Ne(main_thread_id));
}

TEST_F(JavaThreadContextTest, RunOnBackgroundThreadAndCancel) {
  JavaThreadContext java_thread_context(env_);
  ThreadContext thread_context(&java_thread_context);
  pthread_t main_thread_id = thread_context.thread_id();
  RunOnBackgroundThread(env_, ThreadContext::WaitAndStore, &thread_context,
                        ThreadContext::CancelStore, &java_thread_context);
  thread_context.WaitForStart();
  java_thread_context.Cancel();
  thread_context.Continue();
  thread_context.WaitForCompletion();
  EXPECT_THAT(thread_context.thread_id(), Eq(main_thread_id));
  EXPECT_TRUE(thread_context.canceled());
  EXPECT_TRUE(thread_context.cancel_store_called());
}

/***** JavaObjectToVariant test *****/
class JavaObjectToVariantTest : public ::testing::Test {
 protected:
  void SetUp() override {
    env_ = firebase::testing::cppsdk::GetTestJniEnv();
    ASSERT_TRUE(env_ != nullptr);
    activity_ = firebase::testing::cppsdk::GetTestActivity();
    ASSERT_TRUE(activity_ != nullptr);
    ASSERT_TRUE(Initialize(env_, activity_));
  }

  void TearDown() override {
    ASSERT_TRUE(env_ != nullptr);
    Terminate(env_);
  }

  const int kTestValueInt = 0x01234567;
  const int64 kTestValueLong = 0x1234567ABCD1234L;
  const int16 kTestValueShort = 0x3456;
  const char kTestValueByte = 0x12;
  const bool kTestValueBool = true;
  const char *const kTestValueString = "Hello, world!";
  const float kTestValueFloat = 0.15625f;
  const double kTestValueDouble = 1048576.15625;

  JNIEnv *env_;
  jobject activity_;
};

TEST_F(JavaObjectToVariantTest, TestFundamentalTypes) {
  // null converts to Variant::kTypeNull.
  {
    jobject obj = nullptr;
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj), Variant::Null());
  }
  // Integral types convert to Variant::kTypeInt64. This includes Date.
  {
    // Integer
    jobject obj =
        env_->NewObject(integer_class::GetClass(),
                        integer_class::GetMethodId(integer_class::kConstructor),
                        static_cast<jint>(kTestValueInt));
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromInt64(kTestValueInt))
        << "Failed to convert Integer";
    env_->DeleteLocalRef(obj);
  }
  {
    // Short
    jobject obj =
        env_->NewObject(short_class::GetClass(),
                        short_class::GetMethodId(short_class::kConstructor),
                        static_cast<jshort>(kTestValueShort));
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromInt64(kTestValueShort))
        << "Failed to convert Short";
    env_->DeleteLocalRef(obj);
  }
  {
    // Long
    jobject obj =
        env_->NewObject(long_class::GetClass(),
                        long_class::GetMethodId(long_class::kConstructor),
                        static_cast<jlong>(kTestValueLong));
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromInt64(kTestValueLong))
        << "Failed to convert Long";
    env_->DeleteLocalRef(obj);
  }
  {
    // Byte
    jobject obj =
        env_->NewObject(byte_class::GetClass(),
                        byte_class::GetMethodId(byte_class::kConstructor),
                        static_cast<jbyte>(kTestValueByte));
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromInt64(kTestValueByte))
        << "Failed to convert Byte";
    env_->DeleteLocalRef(obj);
  }
  {
    // Date becomes an Int64 of milliseconds since epoch, which is also what the
    // Java Date constructor happens to take as an argument.
    jobject obj = env_->NewObject(date::GetClass(),
                                  date::GetMethodId(date::kConstructorWithTime),
                                  static_cast<jlong>(kTestValueLong));
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromInt64(kTestValueLong))
        << "Failed to convert Date";
    env_->DeleteLocalRef(obj);
  }

  // Floating point types convert to Variant::kTypeDouble.
  {
    // Float
    jobject obj =
        env_->NewObject(float_class::GetClass(),
                        float_class::GetMethodId(float_class::kConstructor),
                        static_cast<jfloat>(kTestValueFloat));
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromDouble(kTestValueFloat))
        << "Failed to convert Float";
    env_->DeleteLocalRef(obj);
  }
  {
    // Double
    jobject obj =
        env_->NewObject(double_class::GetClass(),
                        double_class::GetMethodId(double_class::kConstructor),
                        static_cast<jdouble>(kTestValueDouble));
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromDouble(kTestValueDouble))
        << "Failed to convert Double";
    env_->DeleteLocalRef(obj);
  }
  // Boolean converts to Variant::kTypeBool.
  {
    jobject obj =
        env_->NewObject(boolean_class::GetClass(),
                        boolean_class::GetMethodId(boolean_class::kConstructor),
                        static_cast<jboolean>(kTestValueBool));
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromBool(kTestValueBool))
        << "Failed to convert Boolean";
    env_->DeleteLocalRef(obj);
  }
  // String converts to Variant::kTypeMutableString.
  {
    jobject obj = env_->NewStringUTF(kTestValueString);
    EXPECT_EQ(util::JavaObjectToVariant(env_, obj),
              Variant::FromMutableString(kTestValueString))
        << "Failed to convert String";
    env_->DeleteLocalRef(obj);
  }
}

TEST_F(JavaObjectToVariantTest, TestContainerTypes) {
  // Array and List types convert to Variant::kTypeVector.
  {
    // Two tests in one: Array of Objects, and ArrayList.
    // Both contain {Integer, Float, String, Null}.

    jobjectArray array = env_->NewObjectArray(4, object::GetClass(), nullptr);
    jobject container =
        env_->NewObject(array_list::GetClass(),
                        array_list::GetMethodId(array_list::kConstructor));
    {
      // Element 1: Integer
      jobject obj = env_->NewObject(
          integer_class::GetClass(),
          integer_class::GetMethodId(integer_class::kConstructor),
          static_cast<jint>(kTestValueInt));
      env_->SetObjectArrayElement(array, 0, obj);
      env_->CallBooleanMethod(container,
                              array_list::GetMethodId(array_list::kAdd), obj);
      env_->DeleteLocalRef(obj);
    }
    {
      // Element 2: Float
      jobject obj =
          env_->NewObject(float_class::GetClass(),
                          float_class::GetMethodId(float_class::kConstructor),
                          static_cast<jfloat>(kTestValueFloat));
      env_->SetObjectArrayElement(array, 1, obj);
      env_->CallBooleanMethod(container,
                              array_list::GetMethodId(array_list::kAdd), obj);
      env_->DeleteLocalRef(obj);
    }
    {
      // Element 3: String
      jobject obj = env_->NewStringUTF(kTestValueString);
      env_->SetObjectArrayElement(array, 2, obj);
      env_->CallBooleanMethod(container,
                              array_list::GetMethodId(array_list::kAdd), obj);
      env_->DeleteLocalRef(obj);
    }
    {
      // Element 4: Null
      jobject obj = nullptr;
      env_->SetObjectArrayElement(array, 3, obj);
      env_->CallBooleanMethod(container,
                              array_list::GetMethodId(array_list::kAdd), obj);
    }

    Variant expected = Variant::EmptyVector();
    expected.vector().push_back(Variant::FromInt64(kTestValueInt));
    expected.vector().push_back(Variant::FromDouble(kTestValueFloat));
    expected.vector().push_back(Variant::FromMutableString(kTestValueString));
    expected.vector().push_back(Variant::Null());

    EXPECT_EQ(util::JavaObjectToVariant(env_, array), expected)
        << "Failed to convert Array of Object{Integer, Float, String, Null}";
    EXPECT_EQ(util::JavaObjectToVariant(env_, container), expected)
        << "Failed to convert ArrayList{Integer, Float, String, Null}";
    env_->DeleteLocalRef(array);
    env_->DeleteLocalRef(container);
  }
  // Map type converts to Variant::kTypeMap.
  {
    // Test a HashMap of String to {Integer, Float, String, Null}
    // Only test keys that are strings, as that's all Java provides.
    jobject container = env_->NewObject(
        hash_map::GetClass(), hash_map::GetMethodId(hash_map::kConstructor));
    {
      // Element 1: Integer
      jobject key = env_->NewStringUTF("one");
      jobject obj = env_->NewObject(
          integer_class::GetClass(),
          integer_class::GetMethodId(integer_class::kConstructor),
          static_cast<jint>(kTestValueInt));
      jobject discard = env_->CallObjectMethod(
          container, map::GetMethodId(map::kPut), key, obj);
      env_->DeleteLocalRef(discard);
      env_->DeleteLocalRef(key);
      env_->DeleteLocalRef(obj);
    }
    {
      // Element 2: Float
      jobject key = env_->NewStringUTF("two");
      jobject obj =
          env_->NewObject(float_class::GetClass(),
                          float_class::GetMethodId(float_class::kConstructor),
                          static_cast<jfloat>(kTestValueFloat));
      jobject discard = env_->CallObjectMethod(
          container, map::GetMethodId(map::kPut), key, obj);
      env_->DeleteLocalRef(discard);
      env_->DeleteLocalRef(key);
      env_->DeleteLocalRef(obj);
    }
    {
      // Element 3: String
      jobject key = env_->NewStringUTF("three");
      jobject obj = env_->NewStringUTF(kTestValueString);
      jobject discard = env_->CallObjectMethod(
          container, map::GetMethodId(map::kPut), key, obj);
      env_->DeleteLocalRef(discard);
      env_->DeleteLocalRef(key);
      env_->DeleteLocalRef(obj);
    }
    {
      // Element 4: Null
      jobject key = env_->NewStringUTF("four");
      jobject obj = nullptr;
      jobject discard = env_->CallObjectMethod(
          container, map::GetMethodId(map::kPut), key, obj);
      env_->DeleteLocalRef(discard);
      env_->DeleteLocalRef(key);
    }

    Variant expected = Variant::EmptyMap();
    expected.map()[Variant("one")] = Variant::FromInt64(kTestValueInt);
    expected.map()[Variant("two")] = Variant::FromDouble(kTestValueFloat);
    expected.map()[Variant("three")] =
        Variant::FromMutableString(kTestValueString);
    expected.map()[Variant("four")] = Variant::Null();

    EXPECT_EQ(util::JavaObjectToVariant(env_, container), expected)
        << "Failed to convert Map of String to {Integer, Float, String, Null}";
    env_->DeleteLocalRef(container);
  }
  // TODO(b/113619056): Test complex containers containing other containers.
}

// TODO(b/113619056): Tests for VariantToJavaObject.

}  // namespace util
}  // namespace firebase
