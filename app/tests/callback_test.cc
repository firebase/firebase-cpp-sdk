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

#include "app/src/callback.h"

#include <string>
#include <utility>

#include "app/memory/unique_ptr.h"
#include "app/src/mutex.h"
#include "app/src/thread.h"
#include "app/src/time.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Eq;

namespace firebase {

class CallbackTest : public ::testing::Test {
 protected:
  CallbackTest() {}

  void SetUp() override {
    callback_void_count_ = 0;
    callback1_count_ = 0;
    callback_value1_sum_ = 0;
    callback_value1_ordered_.clear();
    callback_value2_sum_ = 0;
    callback_string_.clear();
    value_and_string_ = std::pair<int, std::string>();
  }

  // Counts callbacks from callback::CallbackVoid.
  static void CountCallbackVoid() { callback_void_count_++; }
  // Counts callbacks from callback::Callback1.
  static void CountCallback1(void* test) {
    CallbackTest* callback_test = *(static_cast<CallbackTest**>(test));
    callback_test->callback1_count_++;
  }
  // Adds the value passed to CallbackValue1 to callback_value1_sum_.
  static void SumCallbackValue1(int value) { callback_value1_sum_ += value; }

  // Add the value passed to CallbackValue1 to callback_value1_ordered_.
  static void OrderedCallbackValue1(int value) {
    callback_value1_ordered_.push_back(value);
  }

  // Multiplies values passed to CallbackValue2 and adds them to
  // callback_value2_sum_.
  static void SumCallbackValue2(char value1, int value2) {
    callback_value2_sum_ += value1 * value2;
  }

  // Appends the string passed to this method to callback_string_.
  static void AggregateCallbackString(const char* str) {
    callback_string_ += str;
  }

  // Stores this function's arguments in value_and_string_.
  static void StoreValueAndString(int value, const char* str) {
    value_and_string_ = std::pair<int, std::string>(value, str);
  }

  // Stores the value argument in value_and_string_.first, then appends the two
  // string arguments and assign to value_and_string_.second,
  static void StoreValueAndString2(const char* str1, const char* str2,
                                   int value) {
    value_and_string_ = std::pair<int, std::string>(
        value, std::string(str1) + std::string(str2));
  }

  // Stores the sum of value1 and value2 in value_and_string_.first and the
  // string argumene in value_and_string_.second.
  static void StoreValue2AndString(char value1, int value2, const char* str) {
    value_and_string_ = std::pair<int, std::string>(value1 + value2, str);
  }

  // Adds the value passed to CallbackMoveValue1 to callback_value1_sum_.
  static void SumCallbackMoveValue1(UniquePtr<int>* value) {
    callback_value1_sum_ += **value;
  }

  int callback1_count_;

  static int callback_void_count_;
  static int callback_value1_sum_;
  static std::vector<int> callback_value1_ordered_;
  static int callback_value2_sum_;
  static std::string callback_string_;
  static std::pair<int, std::string> value_and_string_;
};

int CallbackTest::callback_value1_sum_;
std::vector<int> CallbackTest::callback_value1_ordered_;  // NOLINT
int CallbackTest::callback_value2_sum_;
int CallbackTest::callback_void_count_;
std::string CallbackTest::callback_string_;                   // NOLINT
std::pair<int, std::string> CallbackTest::value_and_string_;  // NOLINT

// Verify initialize and terminate setup and tear down correctly.
TEST_F(CallbackTest, TestInitializeAndTerminate) {
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
  callback::Initialize();
  EXPECT_THAT(callback::IsInitialized(), Eq(true));
  callback::Terminate(false);
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Verify Terminate() is a no-op if the API isn't initialized.
TEST_F(CallbackTest, TestTerminateWithoutInitialization) {
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
  callback::Terminate(false);
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Add a callback to the queue then terminate the API.
TEST_F(CallbackTest, AddCallbackNoInitialization) {
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  EXPECT_THAT(callback::IsInitialized(), Eq(true));
  callback::Terminate(false);
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Flush all callbacks.
TEST_F(CallbackTest, AddCallbacksTerminateAndFlush) {
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  EXPECT_THAT(callback::IsInitialized(), Eq(true));
  callback::PollCallbacks();
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  callback::Terminate(true);
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
  callback::PollCallbacks();
  EXPECT_THAT(callback_void_count_, Eq(1));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Add a callback to the queue, then remove it.  This should result in
// initializing the callback API then tearing it down when the queue is empty.
TEST_F(CallbackTest, AddRemoveCallback) {
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
  void* callback_reference =
      callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  EXPECT_THAT(callback::IsInitialized(), Eq(true));
  callback::RemoveCallback(callback_reference);
  callback::PollCallbacks();
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
  EXPECT_THAT(callback_void_count_, Eq(0));
}

// Call a void callback.
TEST_F(CallbackTest, CallVoidCallback) {
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  callback::PollCallbacks();
  EXPECT_THAT(callback_void_count_, Eq(1));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Call two void callbacks.
TEST_F(CallbackTest, CallTwoVoidCallbacks) {
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  callback::PollCallbacks();
  EXPECT_THAT(callback_void_count_, Eq(2));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Call three void callbacks with a poll between them.
TEST_F(CallbackTest, CallOneVoidCallbackPollTwo) {
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  callback::PollCallbacks();
  EXPECT_THAT(callback_void_count_, Eq(1));
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  callback::AddCallback(new callback::CallbackVoid(CountCallbackVoid));
  callback::PollCallbacks();
  EXPECT_THAT(callback_void_count_, Eq(3));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Call 2, 1 argument callbacks.
TEST_F(CallbackTest, CallCallback1) {
  callback::AddCallback(
      new callback::Callback1<CallbackTest*>(this, CountCallback1));
  callback::AddCallback(
      new callback::Callback1<CallbackTest*>(this, CountCallback1));
  callback::PollCallbacks();
  EXPECT_THAT(callback1_count_, Eq(2));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Call a callback passing the argument by value.
TEST_F(CallbackTest, CallCallbackValue1) {
  callback::AddCallback(
      new callback::CallbackValue1<int>(10, SumCallbackValue1));
  callback::AddCallback(
      new callback::CallbackValue1<int>(5, SumCallbackValue1));
  callback::PollCallbacks();
  EXPECT_THAT(callback_value1_sum_, Eq(15));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Ensure callbacks are executed in the order they're added to the queue.
TEST_F(CallbackTest, CallCallbackValue1Ordered) {
  callback::AddCallback(
      new callback::CallbackValue1<int>(10, OrderedCallbackValue1));
  callback::AddCallback(
      new callback::CallbackValue1<int>(5, OrderedCallbackValue1));
  callback::PollCallbacks();
  std::vector<int> expected;
  expected.push_back(10);
  expected.push_back(5);
  EXPECT_THAT(callback_value1_ordered_, Eq(expected));
}

// Schedule 3 callbacks, removing the middle one from the queue.
TEST_F(CallbackTest, ScheduleThreeCallbacksRemoveOne) {
  callback::AddCallback(
      new callback::CallbackValue1<int>(1, SumCallbackValue1));
  void* reference = callback::AddCallback(
      new callback::CallbackValue1<int>(2, SumCallbackValue1));
  callback::AddCallback(
      new callback::CallbackValue1<int>(4, SumCallbackValue1));
  callback::RemoveCallback(reference);
  callback::PollCallbacks();
  EXPECT_THAT(callback_value1_sum_, Eq(5));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Call a callback passing two arguments by value.
TEST_F(CallbackTest, CallCallbackValue2) {
  callback::AddCallback(
      new callback::CallbackValue2<char, int>(10, 4, SumCallbackValue2));
  callback::AddCallback(
      new callback::CallbackValue2<char, int>(20, 3, SumCallbackValue2));
  callback::PollCallbacks();
  EXPECT_THAT(callback_value2_sum_, Eq(100));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Call a callback passing a string by value.
TEST_F(CallbackTest, CallCallbackString) {
  callback::AddCallback(
      new callback::CallbackString("testing", AggregateCallbackString));
  callback::AddCallback(
      new callback::CallbackString("123", AggregateCallbackString));
  callback::PollCallbacks();
  EXPECT_THAT(callback_string_, Eq("testing123"));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

// Call a callback passing a value and string by value.
TEST_F(CallbackTest, CallCallbackValue1String1) {
  callback::AddCallback(
      new callback::CallbackValue1String1<int>(10, "ten", StoreValueAndString));
  callback::PollCallbacks();
  EXPECT_THAT(value_and_string_.first, Eq(10));
  EXPECT_THAT(value_and_string_.second, Eq("ten"));
}

// Call a callback passing a value and two strings by value.
TEST_F(CallbackTest, CallCallbackString2Value1) {
  callback::AddCallback(new callback::CallbackString2Value1<int>(
      "evening", "all", 11, StoreValueAndString2));
  callback::PollCallbacks();
  EXPECT_THAT(value_and_string_.first, Eq(11));
  EXPECT_THAT(value_and_string_.second, Eq("eveningall"));
}

// Call a callback passing two values and a string by value.
TEST_F(CallbackTest, CallCallbackValue2String1) {
  callback::AddCallback(new callback::CallbackValue2String1<char, int>(
      11, 31, "meaning", StoreValue2AndString));
  callback::PollCallbacks();
  EXPECT_THAT(value_and_string_.first, Eq(42));
  EXPECT_THAT(value_and_string_.second, Eq("meaning"));
}

// Call a callback passing the UniquePtr
TEST_F(CallbackTest, CallCallbackMoveValue1) {
  callback::AddCallback(new callback::CallbackMoveValue1<UniquePtr<int>>(
      MakeUnique<int>(10), SumCallbackMoveValue1));
  UniquePtr<int> ptr(new int(5));
  callback::AddCallback(new callback::CallbackMoveValue1<UniquePtr<int>>(
      Move(ptr), SumCallbackMoveValue1));
  callback::PollCallbacks();
  EXPECT_THAT(callback_value1_sum_, Eq(15));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

#ifdef FIREBASE_USE_STD_FUNCTION
// Call a callback which wraps std::function
TEST_F(CallbackTest, CallCallbackStdFunction) {
  int count = 0;
  std::function<void()> callback = [&count]() { count++; };

  callback::AddCallback(new callback::CallbackStdFunction(callback));
  callback::PollCallbacks();
  EXPECT_THAT(count, Eq(1));
  callback::AddCallback(new callback::CallbackStdFunction(callback));
  callback::AddCallback(new callback::CallbackStdFunction(callback));
  callback::PollCallbacks();
  EXPECT_THAT(count, Eq(3));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}
#endif

// Ensure callbacks are executed in the order they're added to the queue with
// callbacks added to a different thread to the dispatching thread.
// Also, make sure it's possible to remove a callback from the queue while
// executing a callback.
TEST_F(CallbackTest, ThreadedCallbackValue1Ordered) {
  bool running = true;
  void* callback_entry_to_remove = nullptr;
  Thread pollingThread(
      [](void* arg) -> void {
        volatile bool* running_ptr = static_cast<bool*>(arg);
        while (*running_ptr) {
          callback::PollCallbacks();
          // Wait 20ms.
          ::firebase::internal::Sleep(20);
        }
      },
      &running);
  Thread addCallbacksThread(
      [](void* arg) -> void {
        void** callback_entry_to_remove_ptr = static_cast<void**>(arg);
        callback::AddCallback(
            new callback::CallbackValue1<int>(1, OrderedCallbackValue1));
        callback::AddCallback(
            new callback::CallbackValue1<int>(2, OrderedCallbackValue1));
        // Adds a callback which removes the entry referenced by
        // callback_entry_to_remove.
        callback::AddCallback(new callback::CallbackValue1<void**>(
            callback_entry_to_remove_ptr, [](void** callback_entry) -> void {
              callback::RemoveCallback(*callback_entry);
            }));
        *callback_entry_to_remove_ptr = callback::AddCallback(
            new callback::CallbackValue1<int>(4, OrderedCallbackValue1));
        callback::AddCallback(
            new callback::CallbackValue1<int>(5, OrderedCallbackValue1));
      },
      &callback_entry_to_remove);
  addCallbacksThread.Join();
  callback::AddCallback(new callback::CallbackValue1<volatile bool*>(
      &running, [](volatile bool* running_ptr) { *running_ptr = false; }));
  pollingThread.Join();
  std::vector<int> expected;
  expected.push_back(1);
  expected.push_back(2);
  expected.push_back(5);
  EXPECT_THAT(callback_value1_ordered_, Eq(expected));
}

TEST_F(CallbackTest, NewCallbackTest) {
  callback::AddCallback(callback::NewCallback(SumCallbackValue1, 1));
  callback::AddCallback(callback::NewCallback(SumCallbackValue1, 2));
  callback::AddCallback(
      callback::NewCallback(SumCallbackValue2, static_cast<char>(1), 10));
  callback::AddCallback(
      callback::NewCallback(SumCallbackValue2, static_cast<char>(2), 100));
  callback::AddCallback(
      callback::NewCallback(AggregateCallbackString, "Hello, "));
  callback::AddCallback(
      callback::NewCallback(AggregateCallbackString, "World!"));
  callback::PollCallbacks();
  EXPECT_THAT(callback_value1_sum_, Eq(3));
  EXPECT_THAT(callback_value2_sum_, Eq(210));
  EXPECT_THAT(callback_string_, testing::StrEq("Hello, World!"));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

TEST_F(CallbackTest, AddCallbackWithThreadCheckTest) {
  // When PollCallbacks() is called in previous test, g_callback_thread_id
  // would be set to current thread which runs the tests.  We want it to be set
  // to a different thread id in the beginning of this test.
  Thread changeThreadIdThread([]() {
    callback::AddCallback(new callback::CallbackVoid([](){}));
    callback::PollCallbacks();
  });
  changeThreadIdThread.Join();
  EXPECT_THAT(callback::IsInitialized(), Eq(false));

  void* entry_non_null = callback::AddCallbackWithThreadCheck(
      new callback::CallbackVoid(CountCallbackVoid));
  EXPECT_TRUE(entry_non_null != nullptr);
  EXPECT_THAT(callback_void_count_, Eq(0));
  EXPECT_THAT(callback::IsInitialized(), Eq(true));

  callback::PollCallbacks();
  EXPECT_THAT(callback_void_count_, Eq(1));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));

  // Once PollCallbacks() is called on this thread, AddCallbackWithThreadCheck()
  // should run the callback immediately and return nullptr.
  void* entry_null = callback::AddCallbackWithThreadCheck(
      new callback::CallbackVoid(CountCallbackVoid));
  EXPECT_TRUE(entry_null == nullptr);
  EXPECT_THAT(callback_void_count_, Eq(2));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));

  callback::PollCallbacks();
  EXPECT_THAT(callback_void_count_, Eq(2));
  EXPECT_THAT(callback::IsInitialized(), Eq(false));
}

TEST_F(CallbackTest, CallbackDeadlockTest) {
  // This is to test the deadlock scenario when CallbackEntry::Execute() and
  // CallbackEntry::DisableCallback() are called at the same time.
  // Ex. given a user mutex "user_mutex"
  // GC thread: lock(user_mutex) -> lock(CallbackEntry::mutex_)
  // Polling thread: lock(CallbackEntry::mutex_) -> lock(user_mutex)
  // If both threads successfully obtain the first lock, a deadlock could occur.
  // CallbackEntry::mutex_ should be released while running the callback.

  struct DeadlockData {
    Mutex user_mutex;
    void* handle;
  };

  for (int i = 0; i < 1000; ++i) {
    DeadlockData data;

    data.handle =
        callback::AddCallback(new callback::CallbackValue1<DeadlockData*>(
            &data, [](DeadlockData* data) {
              MutexLock lock(data->user_mutex);
              data->handle = nullptr;
            }));

    Thread pollingThread([]() { callback::PollCallbacks(); });

    Thread gcThread(
        [](void* arg) {
          DeadlockData* data = static_cast<DeadlockData*>(arg);
          MutexLock lock(data->user_mutex);
          if (data->handle) {
            callback::RemoveCallback(data->handle);
          }
        },
        &data);

    pollingThread.Join();
    gcThread.Join();
    EXPECT_THAT(callback::IsInitialized(), Eq(false));
  }
}
}  // namespace firebase
