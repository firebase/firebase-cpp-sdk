/*
 * Copyright 2018 Google LLC
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

#include "app/src/optional.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

// Using Mocks to count the number of times a constructor, destructor,
// or (move|copy) (constructor|assignment operator) is called is not easy to do
// directly because gMock requires marking the methods to be called virtual.
// Instead, we create a special wrapper Mock object that calls these virtual
// functions instead, so that they can be counted.
class SpecialFunctionsNotifier {
 public:
  virtual ~SpecialFunctionsNotifier() {}

  virtual void Construct() = 0;
  virtual void Copy() = 0;
#ifdef FIREBASE_USE_MOVE_OPERATORS
  virtual void Move() = 0;
#endif
  virtual void Destruct() = 0;
};

// This class exists only to call through to the underlying
// SpecialFunctionNotifier so that those calls can be counted.
class SpecialFunctionsNotifierWrapper {
 public:
  SpecialFunctionsNotifierWrapper() { s_notifier_->Construct(); }
  SpecialFunctionsNotifierWrapper(
      const SpecialFunctionsNotifierWrapper& other) {
    s_notifier_->Copy();
  }
  SpecialFunctionsNotifierWrapper& operator=(
      const SpecialFunctionsNotifierWrapper& other) {
    s_notifier_->Copy();
    return *this;
  }

#ifdef FIREBASE_USE_MOVE_OPERATORS
  SpecialFunctionsNotifierWrapper(SpecialFunctionsNotifierWrapper&& other) {
    s_notifier_->Move();
  }
  SpecialFunctionsNotifierWrapper& operator=(
      SpecialFunctionsNotifierWrapper&& other) {
    s_notifier_->Move();
    return *this;
  }
#endif

  ~SpecialFunctionsNotifierWrapper() { s_notifier_->Destruct(); }

  static SpecialFunctionsNotifier* s_notifier_;
};

SpecialFunctionsNotifier* SpecialFunctionsNotifierWrapper::s_notifier_ =
    nullptr;

class SpecialFunctionsNotifierMock : public SpecialFunctionsNotifier {
 public:
  MOCK_METHOD(void, Construct, (), (override));
  MOCK_METHOD(void, Copy, (), (override));
#ifdef FIREBASE_USE_MOVE_OPERATORS
  MOCK_METHOD(void, Move, (), (override));
#endif
  MOCK_METHOD(void, Destruct, (), (override));
};

// A simple class with a method on it, used for testing the arrow operator of
// Optional<T>.
class IntHolder {
 public:
  explicit IntHolder(int value) : value_(value) {}
  int GetValue() const { return value_; }

 private:
  int value_;
};

// Helper class used to setup mock expect calls due to the complexities of move
// enabled or not
class ExpectCallSetup {
 public:
  explicit ExpectCallSetup(SpecialFunctionsNotifierMock* mock_notifier)
      : mock_notifier_(mock_notifier) {}

  ExpectCallSetup& Construct(size_t expectecCallCount) {
    EXPECT_CALL(*mock_notifier_, Construct()).Times(expectecCallCount);
    return *this;
  }

  ExpectCallSetup& CopyAndMove(size_t expectecCopyCallCount,
                               size_t expectecMoveCallCount) {
#ifdef FIREBASE_USE_MOVE_OPERATORS
    EXPECT_CALL(*mock_notifier_, Copy()).Times(expectecCopyCallCount);
    EXPECT_CALL(*mock_notifier_, Move()).Times(expectecMoveCallCount);
#else
    EXPECT_CALL(*mock_notifier_, Copy()).
        Times(expectecCopyCallCount + expectecMoveCallCount);
#endif
    return *this;
  }

  ExpectCallSetup& Destruct(size_t expectecCallCount) {
    EXPECT_CALL(*mock_notifier_, Destruct()).Times(expectecCallCount);
    return *this;
  }

  SpecialFunctionsNotifierMock* mock_notifier_;
};

class OptionalTest : public ::testing::Test, protected ExpectCallSetup {
 protected:
  OptionalTest()
    : ExpectCallSetup(&mock_notifier_)
  {}

  void SetUp() override {
    SpecialFunctionsNotifierWrapper::s_notifier_ = &mock_notifier_;
  }

  void TearDown() override {
    SpecialFunctionsNotifierWrapper::s_notifier_ = nullptr;
  }

  ExpectCallSetup& SetupExpectCall() {
    return *this;
  }

  SpecialFunctionsNotifierMock mock_notifier_;
};

TEST_F(OptionalTest, DefaultConstructor) {
  Optional<int> optional_int;
  EXPECT_FALSE(optional_int.has_value());

  Optional<SpecialFunctionsNotifierWrapper> optional_struct;
  EXPECT_FALSE(optional_struct.has_value());
}

TEST_F(OptionalTest, CopyConstructor) {
  Optional<int> optional_int(9999);

  Optional<int> copy_of_optional_int(optional_int);
  EXPECT_TRUE(copy_of_optional_int.has_value());
  EXPECT_EQ(copy_of_optional_int.value(), 9999);

  Optional<int> another_copy_of_optional_int = optional_int;
  EXPECT_TRUE(another_copy_of_optional_int.has_value());
  EXPECT_EQ(another_copy_of_optional_int.value(), 9999);

  SetupExpectCall()
    .Construct(1)
    .CopyAndMove(2, 1)
    .Destruct(4);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct(
      SpecialFunctionsNotifierWrapper{});

  Optional<SpecialFunctionsNotifierWrapper> copy_of_optional_struct(
      optional_struct);
  EXPECT_TRUE(copy_of_optional_struct.has_value());

  Optional<SpecialFunctionsNotifierWrapper> another_copy_of_optional_struct =
      optional_struct;
  EXPECT_TRUE(another_copy_of_optional_struct.has_value());
}

TEST_F(OptionalTest, CopyAssignment) {
  Optional<int> optional_int(9999);
  Optional<int> another_optional_int(42);
  another_optional_int = optional_int;
  EXPECT_TRUE(optional_int.has_value());
  EXPECT_EQ(optional_int.value(), 9999);
  EXPECT_TRUE(another_optional_int.has_value());
  EXPECT_EQ(another_optional_int.value(), 9999);

  SetupExpectCall()
    .Construct(2)
    .CopyAndMove(1, 2)
    .Destruct(4);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct(
      SpecialFunctionsNotifierWrapper{});
  Optional<SpecialFunctionsNotifierWrapper> another_optional_struct(
      SpecialFunctionsNotifierWrapper{});
  another_optional_struct = optional_struct;
  EXPECT_TRUE(optional_struct.has_value());
  EXPECT_TRUE(another_optional_struct.has_value());
}

TEST_F(OptionalTest, CopyAssignmentSelf) {
  Optional<int> optional_int(9999);
  optional_int = *&optional_int;
  EXPECT_TRUE(optional_int.has_value());
  EXPECT_EQ(optional_int.value(), 9999);

  SetupExpectCall()
    .Construct(1)
    .CopyAndMove(1, 1)
    .Destruct(2);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct(
      SpecialFunctionsNotifierWrapper{});
  optional_struct = *&optional_struct;
  EXPECT_TRUE(optional_struct.has_value());
}

TEST_F(OptionalTest, MoveConstructor) {
  Optional<int> optional_int(9999);

  Optional<int> moved_optional_int(std::move(optional_int));
  EXPECT_TRUE(moved_optional_int.has_value());
  EXPECT_EQ(moved_optional_int.value(), 9999);

  Optional<int> another_moved_optional_int = std::move(moved_optional_int);
  EXPECT_TRUE(another_moved_optional_int.has_value());
  EXPECT_EQ(another_moved_optional_int.value(), 9999);

  SetupExpectCall()
    .Construct(1)
    .CopyAndMove(0, 3)
    .Destruct(4);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct(
      SpecialFunctionsNotifierWrapper{});

  Optional<SpecialFunctionsNotifierWrapper> copy_of_optional_struct(
      std::move(optional_struct));
  EXPECT_TRUE(copy_of_optional_struct.has_value());

  Optional<SpecialFunctionsNotifierWrapper> another_copy_of_optional_struct =
      std::move(copy_of_optional_struct);
  EXPECT_TRUE(another_copy_of_optional_struct.has_value());
}

TEST_F(OptionalTest, MoveAssignment) {
  Optional<int> optional_int(9999);
  Optional<int> another_optional_int(42);
  another_optional_int = std::move(optional_int);

  EXPECT_TRUE(another_optional_int.has_value());
  EXPECT_EQ(another_optional_int.value(), 9999);

  SetupExpectCall()
    .Construct(2)
    .CopyAndMove(0, 3)
    .Destruct(4);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct(
      SpecialFunctionsNotifierWrapper{});
  Optional<SpecialFunctionsNotifierWrapper> another_optional_struct(
      SpecialFunctionsNotifierWrapper{});
  another_optional_struct = std::move(optional_struct);

  EXPECT_TRUE(another_optional_struct.has_value());
}

TEST_F(OptionalTest, Destructor) {
  SetupExpectCall()
    .Construct(2)
    .CopyAndMove(0, 2)
    .Destruct(4);

  // Verify the destructor is called when object goes out of scope.
  {
    Optional<SpecialFunctionsNotifierWrapper> optional_struct(
        SpecialFunctionsNotifierWrapper{});
    (void)optional_struct;
  }
  // Verify the destructor is called when reset is called.
  {
    Optional<SpecialFunctionsNotifierWrapper> optional_struct(
        SpecialFunctionsNotifierWrapper{});
    optional_struct.reset();
  }
}

TEST_F(OptionalTest, ValueConstructor) {
  Optional<int> optional_int(1337);
  EXPECT_TRUE(optional_int.has_value());
  EXPECT_EQ(optional_int.value(), 1337);

  SetupExpectCall()
    .Construct(1)
    .CopyAndMove(1, 0)
    .Destruct(2);

  SpecialFunctionsNotifierWrapper value{};
  Optional<SpecialFunctionsNotifierWrapper> optional_struct(value);
  EXPECT_TRUE(optional_struct.has_value());
}

TEST_F(OptionalTest, ValueMoveConstructor) {
  SetupExpectCall()
    .Construct(1)
    .CopyAndMove(0, 1)
    .Destruct(2);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct(
      SpecialFunctionsNotifierWrapper{});
  EXPECT_TRUE(optional_struct.has_value());
}

TEST_F(OptionalTest, ValueCopyAssignmentToUnpopulatedOptional) {
  Optional<int> optional_int;
  optional_int = 9999;
  EXPECT_TRUE(optional_int.has_value());
  EXPECT_EQ(optional_int.value(), 9999);

  SetupExpectCall()
    .Construct(1)
    .CopyAndMove(1, 0)
    .Destruct(2);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct;
  SpecialFunctionsNotifierWrapper my_struct{};
  optional_struct = my_struct;
  EXPECT_TRUE(optional_struct.has_value());
}

TEST_F(OptionalTest, ValueCopyAssignmentToPopulatedOptional) {
  Optional<int> optional_int(27);
  optional_int = 9999;
  EXPECT_TRUE(optional_int.has_value());
  EXPECT_EQ(optional_int.value(), 9999);

  SetupExpectCall()
    .Construct(2)
    .CopyAndMove(1, 1)
    .Destruct(3);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct(
      SpecialFunctionsNotifierWrapper{});
  SpecialFunctionsNotifierWrapper my_struct{};
  optional_struct = my_struct;
  EXPECT_TRUE(optional_struct.has_value());
}

TEST_F(OptionalTest, ValueMoveAssignmentToUnpopulatedOptional) {
  SetupExpectCall()
    .Construct(1)
    .CopyAndMove(0, 1)
    .Destruct(2);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct;
  SpecialFunctionsNotifierWrapper my_struct{};
  optional_struct = std::move(my_struct);
  EXPECT_TRUE(optional_struct.has_value());
}

TEST_F(OptionalTest, ValueMoveAssignmentToPopulatedOptional) {
  SetupExpectCall()
    .Construct(2)
    .CopyAndMove(0, 2)
    .Destruct(3);

  Optional<SpecialFunctionsNotifierWrapper> optional_struct(
      SpecialFunctionsNotifierWrapper{});
  SpecialFunctionsNotifierWrapper my_struct{};
  optional_struct = std::move(my_struct);
  EXPECT_TRUE(optional_struct.has_value());
}

TEST_F(OptionalTest, ArrowOperator) {
  Optional<IntHolder> optional_int_holder(IntHolder(12345));
  EXPECT_EQ(optional_int_holder->GetValue(), 12345);
}

TEST_F(OptionalTest, HasValue) {
  Optional<int> optional_int;
  EXPECT_FALSE(optional_int.has_value());

  optional_int = 12345;
  EXPECT_TRUE(optional_int.has_value());

  optional_int.reset();
  EXPECT_FALSE(optional_int.has_value());
}

TEST_F(OptionalTest, ValueDeathTest) {
  Optional<int> empty;
  EXPECT_DEATH(empty.value(), "");
}

TEST_F(OptionalTest, ValueOr) {
  Optional<int> optional_int;
  EXPECT_EQ(optional_int.value_or(67890), 67890);

  optional_int = 12345;
  EXPECT_EQ(optional_int.value_or(67890), 12345);
}

TEST_F(OptionalTest, EqualityOperator) {
  Optional<int> lhs(123456);
  Optional<int> rhs(123456);
  Optional<int> wrong(654321);
  Optional<int> empty;
  Optional<int> another_empty;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);
  EXPECT_FALSE(lhs == wrong);
  EXPECT_TRUE(lhs != wrong);

  EXPECT_FALSE(empty == rhs);
  EXPECT_TRUE(empty != rhs);
  EXPECT_TRUE(empty == another_empty);
  EXPECT_FALSE(empty != another_empty);
}

TEST_F(OptionalTest, OptionalFromPointer) {
  int value = 100;
  int* value_ptr = &value;
  int* value_nullptr = nullptr;
  Optional<int> optional_with_value = OptionalFromPointer(value_ptr);
  Optional<int> optional_without_value = OptionalFromPointer(value_nullptr);

  EXPECT_TRUE(optional_with_value.has_value());
  EXPECT_EQ(optional_with_value.value(), 100);
  EXPECT_FALSE(optional_without_value.has_value());
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
