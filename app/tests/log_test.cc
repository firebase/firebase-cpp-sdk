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

#include "app/src/log.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
// The test-cases here are by no means exhaustive. We only make sure the log
// code does not break. Whether logs are output is highly device-dependent and
// testing that is not right now the main goal here.

TEST(LogTest, TestSetAndGetLogLevel) {
  // Try to set log-level and verify we get what we set.
  SetLogLevel(kLogLevelDebug);
  EXPECT_EQ(kLogLevelDebug, GetLogLevel());

  SetLogLevel(kLogLevelError);
  EXPECT_EQ(kLogLevelError, GetLogLevel());
}

TEST(LogDeathTest, TestLogAssert) {
  // Try to make assertion and verify it dies.
  SetLogLevel(kLogLevelVerbose);
// Somehow the death test does not work on ios emulator.
#if !defined(__APPLE__)
  EXPECT_DEATH(LogAssert("should die"), "");
#endif  // !defined(__APPLE__)
}

TEST(LogTest, TestLogLevelBelowAssert) {
  // Try other non-aborting log levels.
  SetLogLevel(kLogLevelVerbose);
  // TODO(zxu): Try to catch the logs using log callback in order to verify the
  // log message.
  LogDebug("debug message");
  LogInfo("info message");
  LogWarning("warning message");
  LogError("error message");
}

}  // namespace firebase
