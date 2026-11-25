/*
 * Copyright 2026 Google LLC
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

#include "app/src/function_registry.h"

#include "gtest/gtest.h"

namespace firebase {
namespace internal {
namespace {

bool ReturnTrue(App* /*app*/, void* /*args*/, void* /*out*/) { return true; }

TEST(FunctionRegistryTest, RegisteredFunctionCanBeCalled) {
  FunctionRegistry registry;
  EXPECT_FALSE(registry.FunctionExists(FnAuthGetCurrentToken));
  EXPECT_FALSE(
      registry.CallFunction(FnAuthGetCurrentToken, nullptr, nullptr, nullptr));

  EXPECT_TRUE(registry.RegisterFunction(FnAuthGetCurrentToken, ReturnTrue));
  EXPECT_TRUE(registry.FunctionExists(FnAuthGetCurrentToken));
  EXPECT_TRUE(
      registry.CallFunction(FnAuthGetCurrentToken, nullptr, nullptr, nullptr));
}

TEST(FunctionRegistryTest, RegisterFailsIfAlreadyRegistered) {
  FunctionRegistry registry;
  EXPECT_TRUE(registry.RegisterFunction(FnAuthGetCurrentToken, ReturnTrue));
  EXPECT_FALSE(registry.RegisterFunction(FnAuthGetCurrentToken, ReturnTrue));
}

TEST(FunctionRegistryTest, UnregisterRemovesFunction) {
  FunctionRegistry registry;
  EXPECT_TRUE(registry.RegisterFunction(FnAuthGetCurrentToken, ReturnTrue));

  EXPECT_TRUE(registry.UnregisterFunction(FnAuthGetCurrentToken));
  EXPECT_FALSE(registry.FunctionExists(FnAuthGetCurrentToken));
  EXPECT_FALSE(
      registry.CallFunction(FnAuthGetCurrentToken, nullptr, nullptr, nullptr));

  // Nothing left to remove, and the identifier can be bound again.
  EXPECT_FALSE(registry.UnregisterFunction(FnAuthGetCurrentToken));
  EXPECT_TRUE(registry.RegisterFunction(FnAuthGetCurrentToken, ReturnTrue));
}

}  // namespace
}  // namespace internal
}  // namespace firebase
