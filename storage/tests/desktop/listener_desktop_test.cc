// Copyright 2026 Google LLC
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

#include "storage/src/desktop/listener_desktop.h"

#include <cstring>
#include <new>

#include "gtest/gtest.h"

namespace {

using firebase::storage::internal::ListenerInternal;

// Regression test for https://github.com/firebase/firebase-cpp-sdk/issues/1966.
// A listener that was never attached to an operation must not touch a rest
// operation when it is destroyed. The object is constructed in pre-filled
// storage so that any member the constructor leaves uninitialized holds a
// non-null garbage value.
TEST(ListenerDesktopTest, DestroyWithoutAttachedOperation) {
  alignas(ListenerInternal) unsigned char storage[sizeof(ListenerInternal)];
  std::memset(storage, 0xAB, sizeof(storage));

  ListenerInternal* listener = new (storage) ListenerInternal(nullptr);
  listener->~ListenerInternal();
}

}  // namespace
