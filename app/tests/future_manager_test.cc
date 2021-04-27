/*
 * Copyright 2016 Google LLC
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

#include "app/src/future_manager.h"

#include <stdlib.h>

#include <ctime>
#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/future.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "thread/fiber/fiber.h"
#include "util/random/mt_random_thread_safe.h"

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NotNull;

namespace firebase {
namespace detail {
namespace testing {

enum FutureManagerTestFn { kTestFnOne, kTestFnCount };

class FutureManagerTest : public ::testing::Test {
 protected:
  FutureManager future_manager_;
  int value1_;
  int value2_;
  int value3_;
};

typedef FutureManagerTest FutureManagerDeathTest;

TEST_F(FutureManagerTest, TestAllocFutureApis) {
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());
  EXPECT_THAT(future_manager_.GetFutureApi(&value2_), IsNull());

  future_manager_.AllocFutureApi(&value1_, kTestFnCount);
  future_manager_.AllocFutureApi(&value2_, kTestFnCount);

  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), NotNull());
  EXPECT_THAT(future_manager_.GetFutureApi(&value2_), NotNull());
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_),
              Ne(future_manager_.GetFutureApi(&value2_)));
  EXPECT_THAT(future_manager_.GetFutureApi(&value3_), IsNull());
}

TEST_F(FutureManagerTest, TestMoveFutureApis) {
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());
  EXPECT_THAT(future_manager_.GetFutureApi(&value2_), IsNull());

  future_manager_.AllocFutureApi(&value1_, kTestFnCount);
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), NotNull());
  EXPECT_THAT(future_manager_.GetFutureApi(&value2_), IsNull());

  ReferenceCountedFutureImpl* impl = future_manager_.GetFutureApi(&value1_);
  future_manager_.MoveFutureApi(&value1_, &value2_);
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());
  EXPECT_THAT(future_manager_.GetFutureApi(&value2_), NotNull());
  EXPECT_THAT(future_manager_.GetFutureApi(&value2_), Eq(impl));
}

TEST_F(FutureManagerTest, TestReleaseFutureApi) {
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());

  future_manager_.AllocFutureApi(&value1_, kTestFnCount);
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), NotNull());

  future_manager_.ReleaseFutureApi(&value1_);
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());
}

TEST_F(FutureManagerTest, TestOrphaningFutures) {
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());

  future_manager_.AllocFutureApi(&value1_, kTestFnCount);
  ReferenceCountedFutureImpl* future_impl =
      future_manager_.GetFutureApi(&value1_);
  EXPECT_THAT(future_impl, NotNull());

  auto handle = future_impl->SafeAlloc<void>(kTestFnOne);
  Future<void> future =
      static_cast<const Future<void>&>(future_impl->LastResult(kTestFnOne));
  EXPECT_THAT(future.status(), Eq(kFutureStatusPending));

  future_manager_.ReleaseFutureApi(&value1_);
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());
  EXPECT_THAT(future.status(), Eq(kFutureStatusPending));

  future_impl->Complete(handle, 0, "");
  EXPECT_THAT(future.status(), Eq(kFutureStatusComplete));
}

TEST_F(FutureManagerTest, TestFutureManagerCanBeDeletedByFutureCallback) {
  auto future_manager = MakeUnique<FutureManager>();

  future_manager->AllocFutureApi(&value1_, kTestFnCount);
  ReferenceCountedFutureImpl* future_impl =
      future_manager->GetFutureApi(&value1_);
  auto handle = future_impl->SafeAlloc<int>(kTestFnOne);
  Future<int> future(future_impl, handle.get());

  Semaphore semaphore(0);
  future.OnCompletion([&](const Future<int>&) {
    future_manager = nullptr;
    semaphore.Post();
  });
  future_impl->CompleteWithResult(handle, 0, "", 42);

  semaphore.Wait();
}

TEST_F(FutureManagerTest, TestFutureManagerCanBeDeletedByFutureInParallel) {
  auto future_manager = MakeUnique<FutureManager>();
  future_manager->AllocFutureApi(&value1_, 2);
  ReferenceCountedFutureImpl* future_impl =
      future_manager->GetFutureApi(&value1_);

  Semaphore running_callback_sem(2);

  // Prepare two futures.
  auto handle_a = future_impl->SafeAlloc<void>(0);
  Future<void> future_a(future_impl, handle_a.get());
  future_a.OnCompletion([&](const Future<void>&) {
    running_callback_sem.Post();
    running_callback_sem.Wait();
  });

  auto handle_b = future_impl->SafeAlloc<void>(1);
  Future<void> future_b(future_impl, handle_b.get());
  Semaphore destroyed_future_manager_sem(0);
  future_b.OnCompletion([&](const Future<void>&) {
    running_callback_sem.Post();
    running_callback_sem.Wait();
    // Only destroy the future manager once both callbacks are running, to make
    // their execution as close to each other as possible.
    future_manager = nullptr;

    destroyed_future_manager_sem.Post();
  });

  // Complete both futures in parallel, so that the callbacks run in parallel as
  // well.
  future_impl->Complete(handle_a, 0, "");
  // Using a `std::function` allows easily capturing the two necessary locals.
  std::function<void()> callback_b = [&] {
    future_impl->Complete(handle_b, 0, "");
  };
  firebase::Thread thread(
      [](void* raw_callback_b) {
        auto* callback_b = static_cast<std::function<void()>*>(raw_callback_b);
        (*callback_b)();
      },
      &callback_b);

  destroyed_future_manager_sem.Wait();
  thread.Join();
}

TEST_F(FutureManagerDeathTest, TestCleanupOrphanedFuturesApis) {
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());

  future_manager_.AllocFutureApi(&value1_, kTestFnCount);
  ReferenceCountedFutureImpl* future_impl =
      future_manager_.GetFutureApi(&value1_);
  EXPECT_THAT(future_impl, NotNull());

  auto handle = future_impl->SafeAlloc<void>(kTestFnOne);
  handle.Detach();
  {
    Future<void> future =
        static_cast<const Future<void>&>(future_impl->LastResult(kTestFnOne));
    EXPECT_THAT(future.status(), Eq(kFutureStatusPending));

    future_manager_.ReleaseFutureApi(&value1_);
    EXPECT_THAT(future.status(), Eq(kFutureStatusPending));
  }

  // Future should still be valid even after cleanup since it is still pending.
  future_manager_.CleanupOrphanedFutureApis(false);
  EXPECT_THAT(future_impl->LastResult(kTestFnOne).status(),
              Eq(kFutureStatusPending));

  // Future should no longer be valid after cleanup since it is complete.
  future_impl->Complete(handle, 0, "");
  future_manager_.CleanupOrphanedFutureApis(false);
  EXPECT_DEATH(future_impl->SafeAlloc<void>(kTestFnOne), "SIGSEGV");
}

TEST_F(FutureManagerDeathTest, TestCleanupOrphanedFuturesApisForcefully) {
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());

  future_manager_.AllocFutureApi(&value1_, kTestFnCount);
  ReferenceCountedFutureImpl* future_impl =
      future_manager_.GetFutureApi(&value1_);
  EXPECT_THAT(future_impl, NotNull());

  future_impl->SafeAlloc<void>(kTestFnOne);

  {
    Future<void> future =
        static_cast<const Future<void>&>(future_impl->LastResult(kTestFnOne));
    EXPECT_THAT(future.status(), Eq(kFutureStatusPending));

    future_manager_.ReleaseFutureApi(&value1_);
    EXPECT_THAT(future.status(), Eq(kFutureStatusPending));
  }

  // Future should no longer be valid after force cleanup regardless of whether
  // or not it is complete.
  future_manager_.CleanupOrphanedFutureApis(true);
  EXPECT_DEATH(future_impl->SafeAlloc<void>(kTestFnOne), "SIGSEGV");
}

TEST_F(FutureManagerDeathTest,
       TestCleanupIsNotTriggeredWhileRunningUserCallback) {
  EXPECT_THAT(future_manager_.GetFutureApi(&value1_), IsNull());
  EXPECT_THAT(future_manager_.GetFutureApi(&value2_), IsNull());

  future_manager_.AllocFutureApi(&value1_, kTestFnCount);
  ReferenceCountedFutureImpl* future_impl =
      future_manager_.GetFutureApi(&value1_);
  // The other future api is only allocated so that it can be released in the
  // completion, triggering cleanup.
  future_manager_.AllocFutureApi(&value2_, kTestFnCount);

  auto handle = future_impl->SafeAlloc<int>(kTestFnOne);
  Future<int> future(future_impl, handle.get());

  Semaphore semaphore(0);
  future.OnCompletion([&](const Future<int>& future) {
    // Triggers cleanup of orphaned instances (calls CleanupOrphanedFutureApis
    // under the hood).
    future_manager_.ReleaseFutureApi(&value2_);
    // The future api shouldn't have been cleaned up by the previous line.
    ASSERT_NE(future.status(), kFutureStatusInvalid);
    EXPECT_EQ(*future.result(), 42);

    semaphore.Post();
  });

  future_manager_.ReleaseFutureApi(&value1_);  // Make it orphaned
  // The future API, even though, orphaned, should not have been deallocated,
  // because there is still a pending future associated with it.
  EXPECT_EQ(future.status(), kFutureStatusPending);
  future_impl->CompleteWithResult(handle, 0, "", 42);

  semaphore.Wait();
}

}  // namespace testing
}  // namespace detail
}  // namespace firebase
