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

#include "app/src/uuid.h"

#include <string.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Contains;
using ::testing::Ne;

// Generate a UUID and make sure it's not zero.
TEST(UuidTest, Generate) {
  firebase::internal::Uuid uuid;
  memset(&uuid, 0, sizeof(uuid));
  uuid.Generate();
  EXPECT_THAT(uuid.data, Contains(Ne(0)));
}

// Generate two UUIDs and verify they're different.
TEST(UuidTest, GenerateDifferent) {
  firebase::internal::Uuid uuid[2];
  memset(&uuid, 0, sizeof(uuid));
  uuid[0].Generate();
  uuid[1].Generate();
  EXPECT_THAT(memcmp(uuid[0].data, uuid[1].data, sizeof(uuid[0].data)), Ne(0));
}
