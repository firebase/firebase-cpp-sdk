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
#include "gtest/gtest.h"
#include "gmock/gmock.h"

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

namespace {
class OwnerObject {
 public:
  OwnerObject() { notifier_.RegisterOwner(this); }
  ~OwnerObject() { notifier_.CleanupAll(); }

 protected:
  CleanupNotifier notifier_;
};

class DerivedOwnerObject : public OwnerObject {
 public:
  DerivedOwnerObject() { notifier_.RegisterOwner(this); }
  ~DerivedOwnerObject() {}
};

class SubscriberObject {
 public:
  SubscriberObject(void* subscribe_for_cleanup_object,
                   bool* flag_to_set_on_cleanup)
      : subscribe_for_cleanup_object_(subscribe_for_cleanup_object),
        flag_to_set_on_cleanup_(flag_to_set_on_cleanup) {
    CleanupNotifier* notifier =
        CleanupNotifier::FindByOwner(subscribe_for_cleanup_object_);
    EXPECT_TRUE(notifier != nullptr);
    notifier->RegisterObject(this, [](void* object) {
      delete reinterpret_cast<SubscriberObject*>(object);
    });
  }

  ~SubscriberObject() {
    CleanupNotifier* notifier =
        CleanupNotifier::FindByOwner(subscribe_for_cleanup_object_);
    EXPECT_TRUE(notifier != nullptr);
    notifier->UnregisterObject(this);
    *flag_to_set_on_cleanup_ = true;
  }

 private:
  void* subscribe_for_cleanup_object_;
  bool* flag_to_set_on_cleanup_;
};
}  // namespace

class CleanupNotifierOwnerRegistryTest : public ::testing::Test {};

// Validate registration and retrieval of owner objects.
TEST_F(CleanupNotifierOwnerRegistryTest, RegisterAndFindByOwner) {
  int owner1 = 1;
  int owner2 = 2;
  int owner3 = 3;
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner1));
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner2));
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner3));
  {
    CleanupNotifier notifier1;
    {
      CleanupNotifier notifier2;
      notifier1.RegisterOwner(&owner1);
      notifier1.RegisterOwner(&owner2);
      notifier2.RegisterOwner(&owner2);
      notifier2.RegisterOwner(&owner3);
      EXPECT_EQ(&notifier1, CleanupNotifier::FindByOwner(&owner1));
      EXPECT_EQ(&notifier2, CleanupNotifier::FindByOwner(&owner3));
      // Registration with notifier2 overrides owner2 association with
      // notifier1.
      EXPECT_EQ(&notifier2, CleanupNotifier::FindByOwner(&owner2));
    }
    EXPECT_EQ(&notifier1, CleanupNotifier::FindByOwner(&owner1));
    EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner2));
    EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner3));
  }
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner1));
}

TEST_F(CleanupNotifierOwnerRegistryTest, RegisterAndUnregisterByOwner) {
  int owner1 = 1;
  int owner2 = 2;
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner1));
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner2));
  {
    CleanupNotifier notifier;
    notifier.RegisterOwner(&owner1);
    notifier.RegisterOwner(&owner2);
    EXPECT_EQ(&notifier, CleanupNotifier::FindByOwner(&owner1));
    EXPECT_EQ(&notifier, CleanupNotifier::FindByOwner(&owner2));
    notifier.UnregisterOwner(&owner2);
    EXPECT_EQ(&notifier, CleanupNotifier::FindByOwner(&owner1));
    EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner2));
  }
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(&owner1));
}

TEST_F(CleanupNotifierOwnerRegistryTest, CleanupRegistrationByOwnerObject) {
  void* owner_pointer = nullptr;
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(owner_pointer));
  Object cleanup_object(0);
  {
    OwnerObject owner;
    owner_pointer = &owner;
    // The cleanup notifier is not part of the public API of OwnerObject so we
    // find it via a pointer to the object in the global registry.
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(owner_pointer);
    EXPECT_TRUE(notifier != nullptr);
    notifier->RegisterObject(&cleanup_object, Object::IncrementCounter);
  }
  EXPECT_EQ(1, cleanup_object.counter);
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(owner_pointer));
}

TEST_F(CleanupNotifierOwnerRegistryTest, CleanupRegistrationByDerivedOwner) {
  void* owner_pointer = nullptr;
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(owner_pointer));
  Object cleanup_object(0);
  {
    DerivedOwnerObject derived_owner;
    owner_pointer = &derived_owner;
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(owner_pointer);
    EXPECT_TRUE(notifier != nullptr);
    notifier->RegisterObject(&cleanup_object, Object::IncrementCounter);
  }
  EXPECT_EQ(1, cleanup_object.counter);
  EXPECT_EQ(nullptr, CleanupNotifier::FindByOwner(owner_pointer));
}

TEST_F(CleanupNotifierOwnerRegistryTest,
       CleanupSubscriberObjectOnOwnerDeletion) {
  bool subscriber_deleted = false;
  OwnerObject* owner = new OwnerObject;
  SubscriberObject* subscriber =
      new SubscriberObject(owner, &subscriber_deleted);
  (void)subscriber;
  delete owner;
  EXPECT_TRUE(subscriber_deleted);
}

TEST_F(CleanupNotifierOwnerRegistryTest,
       CleanupSubscriberObjectBeforeOwnerDeletion) {
  bool subscriber_deleted = false;
  OwnerObject owner;
  {
    SubscriberObject subscriber(&owner, &subscriber_deleted);
    (void)subscriber;
  }
  (void)owner;
  EXPECT_TRUE(subscriber_deleted);
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

}  // namespace testing
}  // namespace firebase
