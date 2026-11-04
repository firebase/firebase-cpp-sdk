// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "remote_config/src/cleanup.h"

#include "gtest/gtest.h"

namespace firebase::remote_config {
namespace {

class FakeCleanupOwner {
 public:
 private:};

class TrackableCleanupObject {
 public:
  void Cleanup() {
    ++cleanup_count_;
    cleaned_up_ = true;
  }

  [[nodiscard]] bool cleaned_up() const { return cleaned_up_; }
  [[nodiscard]] int cleanup_count() const { return cleanup_count_; }

 private:
  bool cleaned_up_ = false;
  int cleanup_count_ = 0;
};

using TestCleanupFn = CleanupFn<TrackableCleanupObject, FakeCleanupOwner>;

TEST(RemoteConfigCleanupTest, RegisterAndCleanupInvokesObjectCleanup) {
  TrackableCleanupObject first;
  TrackableCleanupObject second;

  {
    FakeCleanupOwner owner;
    TestCleanupFn::Register(&first, &owner);
    TestCleanupFn::Register(&second, &owner);
    EXPECT_FALSE(first.cleaned_up());
    EXPECT_FALSE(second.cleaned_up());
  }

  EXPECT_TRUE(first.cleaned_up());
  EXPECT_EQ(first.cleanup_count(), 1);
  EXPECT_TRUE(second.cleaned_up());
  EXPECT_EQ(second.cleanup_count(), 1);
}

TEST(RemoteConfigCleanupTest, UnregisterPreventsCleanupInvocation) {
  TrackableCleanupObject registered_obj;
  TrackableCleanupObject unregistered_obj;

  {
    FakeCleanupOwner owner;
    TestCleanupFn::Register(&registered_obj, &owner);
    TestCleanupFn::Register(&unregistered_obj, &owner);
    TestCleanupFn::Unregister(&unregistered_obj, &owner);
  }

  EXPECT_TRUE(registered_obj.cleaned_up());
  EXPECT_EQ(registered_obj.cleanup_count(), 1);
  EXPECT_FALSE(unregistered_obj.cleaned_up());
  EXPECT_EQ(unregistered_obj.cleanup_count(), 0);
}

TEST(RemoteConfigCleanupTest, NullOwnerIsHandledSafely) {
  TrackableCleanupObject obj;
  TestCleanupFn::Register(&obj, nullptr);
  TestCleanupFn::Unregister(&obj, nullptr);
  EXPECT_FALSE(obj.cleaned_up());
  EXPECT_EQ(obj.cleanup_count(), 0);
}

}  // namespace
}  // namespace firebase::remote_config
