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

#include "app/src/cleanup_notifier.h"

#include <atomic>
#include <memory>
#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace testing {

class CleanupNotifierTest : public ::testing::Test {};

namespace {
struct Object {
  explicit Object(int counter_) : counter(counter_) {}
  int counter;

  static void IncrementCounter(void* obj_void) {
    reinterpret_cast<Object*>(obj_void)->counter++;
  }
  static void DecrementCounter(void* obj_void) {
    reinterpret_cast<Object*>(obj_void)->counter--;
  }
};
}  // namespace

TEST_F(CleanupNotifierTest, TestCallbacksAreCalledAutomatically) {
  Object obj(0);
  {
    CleanupNotifier cleanup;
    cleanup.RegisterObject(&obj, Object::IncrementCounter);
    EXPECT_EQ(obj.counter, 0);
  }
  EXPECT_EQ(obj.counter, 1);
}

TEST_F(CleanupNotifierTest, TestCallbacksAreCalledManuallyOnceOnly) {
  Object obj(0);
  {
    CleanupNotifier cleanup;
    cleanup.RegisterObject(&obj, Object::IncrementCounter);
    EXPECT_EQ(obj.counter, 0);
    cleanup.CleanupAll();
    EXPECT_EQ(obj.counter, 1);
    cleanup.CleanupAll();
    EXPECT_EQ(obj.counter, 1);
  }
  // Ensure the callback isn't called again.
  EXPECT_EQ(obj.counter, 1);
}

TEST_F(CleanupNotifierTest, TestCallbacksCanBeUnregistered) {
  Object obj(0);
  {
    CleanupNotifier cleanup;
    cleanup.RegisterObject(&obj, Object::IncrementCounter);
    cleanup.UnregisterObject(&obj);
    EXPECT_EQ(obj.counter, 0);
  }
  EXPECT_EQ(obj.counter, 0);
}

TEST_F(CleanupNotifierTest, TestMultipleObjects) {
  Object obj1(1), obj2(2);
  {
    CleanupNotifier cleanup;
    cleanup.RegisterObject(&obj1, Object::IncrementCounter);
    cleanup.RegisterObject(&obj2, Object::IncrementCounter);
  }
  EXPECT_EQ(obj1.counter, 2);
  EXPECT_EQ(obj2.counter, 3);
}

TEST_F(CleanupNotifierTest, TestMultipleCallbacksMultipleObjects) {
  Object obj1(1), obj2(2);
  {
    CleanupNotifier cleanup;
    cleanup.RegisterObject(&obj1, Object::IncrementCounter);
    cleanup.RegisterObject(&obj2, Object::DecrementCounter);
  }
  EXPECT_EQ(obj1.counter, 2);
  EXPECT_EQ(obj2.counter, 1);
}

TEST_F(CleanupNotifierTest, TestOnlyOneCallbackPerObject) {
  Object obj1(1), obj2(2);
  {
    CleanupNotifier cleanup;
    cleanup.RegisterObject(&obj1, Object::IncrementCounter);
    cleanup.RegisterObject(&obj2, Object::IncrementCounter);
    // The following call overwrites the previous callback on obj1:
    cleanup.RegisterObject(&obj1, Object::DecrementCounter);
    EXPECT_EQ(obj1.counter, 1);  // Has not run.
  }
  EXPECT_EQ(obj1.counter, 0);
  EXPECT_EQ(obj2.counter, 3);
}

TEST_F(CleanupNotifierTest, TestDoesNotCrashWhenYouUnregisterInvalidObject) {
  Object obj(0);
  {
    CleanupNotifier cleanup;
    cleanup.UnregisterObject(&obj);  // Should not crash.
  }
  EXPECT_EQ(obj.counter, 0);
  {
    CleanupNotifier cleanup;
    cleanup.UnregisterObject(&obj);  // Should not crash.
    cleanup.RegisterObject(&obj, Object::IncrementCounter);
  }
  EXPECT_EQ(obj.counter, 1);
}

TEST_F(CleanupNotifierTest, TestDoesNotCrashIfCallingZeroCallbacks) {
  Object obj(0);
  { CleanupNotifier cleanup; }
  {
    CleanupNotifier cleanup;
    cleanup.CleanupAll();
  }
  EXPECT_EQ(obj.counter, 0);
}

TEST_F(CleanupNotifierTest, TestMultipleCleanupNotifiersReferringToSameObject) {
  Object obj(0);
  {
    CleanupNotifier cleanup1, cleanup2;
    cleanup1.RegisterObject(&obj, Object::IncrementCounter);
    cleanup2.RegisterObject(&obj, Object::IncrementCounter);
  }
  EXPECT_EQ(obj.counter, 2);
}

class TypedCleanupNotifierTest : public ::testing::Test {};

namespace {
struct TypedObject {
  explicit TypedObject(int counter_) : counter(counter_) {}
  int counter;

  static void IncrementCounter(TypedObject* obj) { obj->counter++; }
  static void DecrementCounter(TypedObject* obj) { obj->counter--; }
};
}  // namespace

TEST_F(TypedCleanupNotifierTest, TestCallbacksAreCalledAutomatically) {
  TypedObject obj(0);
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.RegisterObject(&obj, TypedObject::IncrementCounter);
    EXPECT_EQ(obj.counter, 0);
  }
  EXPECT_EQ(obj.counter, 1);
}

TEST_F(TypedCleanupNotifierTest, TestCallbacksAreCalledManuallyOnceOnly) {
  TypedObject obj(0);
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.RegisterObject(&obj, TypedObject::IncrementCounter);
    EXPECT_EQ(obj.counter, 0);
    cleanup.CleanupAll();
    EXPECT_EQ(obj.counter, 1);
    cleanup.CleanupAll();
    EXPECT_EQ(obj.counter, 1);
  }
  // Ensure the callback isn't called again.
  EXPECT_EQ(obj.counter, 1);
}

TEST_F(TypedCleanupNotifierTest, TestCallbacksCanBeUnregistered) {
  TypedObject obj(0);
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.RegisterObject(&obj, TypedObject::IncrementCounter);
    cleanup.UnregisterObject(&obj);
    EXPECT_EQ(obj.counter, 0);
  }
  EXPECT_EQ(obj.counter, 0);
}

TEST_F(TypedCleanupNotifierTest, TestMultipleObjects) {
  TypedObject obj1(1), obj2(2);
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.RegisterObject(&obj1, TypedObject::IncrementCounter);
    cleanup.RegisterObject(&obj2, TypedObject::IncrementCounter);
  }
  EXPECT_EQ(obj1.counter, 2);
  EXPECT_EQ(obj2.counter, 3);
}

TEST_F(TypedCleanupNotifierTest, TestMultipleCallbacksMultipleObjects) {
  TypedObject obj1(1), obj2(2);
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.RegisterObject(&obj1, TypedObject::IncrementCounter);
    cleanup.RegisterObject(&obj2, TypedObject::DecrementCounter);
  }
  EXPECT_EQ(obj1.counter, 2);
  EXPECT_EQ(obj2.counter, 1);
}

TEST_F(TypedCleanupNotifierTest, TestOnlyOneCallbackPerObject) {
  TypedObject obj1(1), obj2(2);
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.RegisterObject(&obj1, TypedObject::IncrementCounter);
    cleanup.RegisterObject(&obj2, TypedObject::IncrementCounter);
    // The following call overwrites the previous callback on obj1:
    cleanup.RegisterObject(&obj1, TypedObject::DecrementCounter);
    EXPECT_EQ(obj1.counter, 1);  // Has not run.
  }
  EXPECT_EQ(obj1.counter, 0);
  EXPECT_EQ(obj2.counter, 3);
}

TEST_F(TypedCleanupNotifierTest,
       TestDoesNotCrashWhenYouUnregisterInvalidObject) {
  TypedObject obj(0);
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.UnregisterObject(&obj);  // Should not crash.
  }
  EXPECT_EQ(obj.counter, 0);
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.UnregisterObject(&obj);  // Should not crash.
    cleanup.RegisterObject(&obj, TypedObject::IncrementCounter);
  }
  EXPECT_EQ(obj.counter, 1);
}

TEST_F(TypedCleanupNotifierTest, TestDoesNotCrashIfCallingZeroCallbacks) {
  TypedObject obj(0);
  { TypedCleanupNotifier<TypedObject> cleanup; }
  {
    TypedCleanupNotifier<TypedObject> cleanup;
    cleanup.CleanupAll();
  }
  EXPECT_EQ(obj.counter, 0);
}

TEST_F(TypedCleanupNotifierTest,
       TestMultipleTypedCleanupNotifiersReferringToSameObject) {
  TypedObject obj(0);
  {
    TypedCleanupNotifier<TypedObject> cleanup1, cleanup2;
    cleanup1.RegisterObject(&obj, TypedObject::IncrementCounter);
    cleanup2.RegisterObject(&obj, TypedObject::IncrementCounter);
  }
  EXPECT_EQ(obj.counter, 2);
}

TEST_F(CleanupNotifierTest, ConcurrentCleanupAllAndUnregisterObjectStress) {
  // Exercises concurrent CleanupAll() racing UnregisterObject() followed by
  // immediate object destruction. Under ASan/TSan this catches any UAF if
  // UnregisterObject returns while the object's cleanup callback is in flight.
  struct HeapTarget {
    std::atomic<int> touched{0};
    static void Callback(void* ptr) {
      auto* self = static_cast<HeapTarget*>(ptr);
      self->touched.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::yield();
      self->touched.fetch_add(1, std::memory_order_relaxed);
    }
  };

  for (int iter = 0; iter < 150; ++iter) {
    CleanupNotifier notifier;
    auto target = std::make_unique<HeapTarget>();
    notifier.RegisterObject(target.get(), HeapTarget::Callback);

    std::atomic<bool> go{false};
    std::thread cleaner([&]() {
      while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      notifier.CleanupAll();
    });
    std::thread unregistrar([&]() {
      while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      notifier.UnregisterObject(target.get());
      // Must be safe to destroy target immediately once UnregisterObject
      // returns.
      target.reset();
    });
    go.store(true, std::memory_order_release);
    cleaner.join();
    unregistrar.join();
  }
}

TEST_F(CleanupNotifierTest, ReentrantUnregisterFromCleanupCallback) {
  struct ReentrantTarget {
    CleanupNotifier* notifier;
    int calls = 0;
    static void Callback(void* ptr) {
      auto* self = static_cast<ReentrantTarget*>(ptr);
      ++self->calls;
      self->notifier->UnregisterObject(self);
    }
  };
  CleanupNotifier notifier;
  ReentrantTarget target{&notifier, 0};
  notifier.RegisterObject(&target, ReentrantTarget::Callback);
  notifier.CleanupAll();
  EXPECT_EQ(target.calls, 1);
}

}  // namespace testing
}  // namespace firebase
