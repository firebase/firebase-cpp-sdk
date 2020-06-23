// Copyright 2019 Google LLC
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

#include "app/src/logger.h"

#include <memory>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace internal {
namespace {

static const size_t kBufferSize = 100;

class FakeLogger : public LoggerBase {
 public:
  FakeLogger()
      : logged_message_(),
        logged_message_level_(static_cast<LogLevel>(-1)),
        log_level_(kLogLevelInfo) {}

  void SetLogLevel(LogLevel log_level) override { log_level_ = log_level; }
  LogLevel GetLogLevel() const override { return log_level_; }

  const std::string& logged_message() const { return logged_message_; }
  LogLevel logged_message_level() const { return logged_message_level_; }

 private:
  void LogMessageImplV(LogLevel log_level, const char* format,
                       va_list args) const override {
    logged_message_level_ = log_level;
    char buffer[kBufferSize];
    vsnprintf(buffer, kBufferSize, format, args);
    logged_message_ = buffer;
  }

  mutable std::string logged_message_;
  mutable LogLevel logged_message_level_;

  mutable LogLevel log_level_;
};

TEST(LoggerTest, GetSetLogLevel) {
  Logger logger(nullptr);
  EXPECT_EQ(logger.GetLogLevel(), kLogLevelInfo);
  logger.SetLogLevel(kLogLevelVerbose);
  EXPECT_EQ(logger.GetLogLevel(), kLogLevelVerbose);

  Logger logger2(nullptr, kLogLevelDebug);
  EXPECT_EQ(logger2.GetLogLevel(), kLogLevelDebug);
  logger2.SetLogLevel(kLogLevelInfo);
  EXPECT_EQ(logger2.GetLogLevel(), kLogLevelInfo);
}

TEST(LoggerTest, LogWithEachFunction) {
  FakeLogger logger;

  // Ensure everything gets through.
  logger.SetLogLevel(kLogLevelVerbose);

  logger.LogDebug("LogDebug %i", 1);
  EXPECT_EQ(logger.logged_message_level(), kLogLevelDebug);
  EXPECT_EQ(logger.logged_message(), "LogDebug 1");

  logger.LogInfo("LogInfo %i", 2);
  EXPECT_EQ(logger.logged_message_level(), kLogLevelInfo);
  EXPECT_EQ(logger.logged_message(), "LogInfo 2");

  logger.LogWarning("LogWarning %i", 3);
  EXPECT_EQ(logger.logged_message_level(), kLogLevelWarning);
  EXPECT_EQ(logger.logged_message(), "LogWarning 3");

  logger.LogError("LogError %i", 4);
  EXPECT_EQ(logger.logged_message_level(), kLogLevelError);
  EXPECT_EQ(logger.logged_message(), "LogError 4");

  logger.LogAssert("LogAssert %i", 5);
  EXPECT_EQ(logger.logged_message_level(), kLogLevelAssert);
  EXPECT_EQ(logger.logged_message(), "LogAssert 5");

  logger.LogMessage(kLogLevelInfo, "LogMessage %i", 6);
  EXPECT_EQ(logger.logged_message_level(), kLogLevelInfo);
  EXPECT_EQ(logger.logged_message(), "LogMessage 6");
}

TEST(LoggerTest, FilteringPermissive) {
  FakeLogger logger;

  logger.SetLogLevel(kLogLevelVerbose);

  logger.LogMessage(kLogLevelVerbose, "Verbose log");
  EXPECT_EQ(logger.logged_message(), "Verbose log");

  logger.LogMessage(kLogLevelDebug, "Debug log");
  EXPECT_EQ(logger.logged_message(), "Debug log");

  logger.LogMessage(kLogLevelInfo, "Info log");
  EXPECT_EQ(logger.logged_message(), "Info log");

  logger.LogMessage(kLogLevelWarning, "Warning log");
  EXPECT_EQ(logger.logged_message(), "Warning log");

  logger.LogMessage(kLogLevelError, "Error log");
  EXPECT_EQ(logger.logged_message(), "Error log");

  logger.LogMessage(kLogLevelAssert, "Assert log");
  EXPECT_EQ(logger.logged_message(), "Assert log");
}

TEST(LoggerTest, FilteringMiddling) {
  FakeLogger logger;

  logger.SetLogLevel(kLogLevelWarning);

  logger.LogMessage(kLogLevelVerbose, "Verbose log");
  EXPECT_EQ(logger.logged_message(), "");

  logger.LogMessage(kLogLevelDebug, "Debug log");
  EXPECT_EQ(logger.logged_message(), "");

  logger.LogMessage(kLogLevelInfo, "Info log");
  EXPECT_EQ(logger.logged_message(), "");

  logger.LogMessage(kLogLevelWarning, "Warning log");
  EXPECT_EQ(logger.logged_message(), "Warning log");

  logger.LogMessage(kLogLevelError, "Error log");
  EXPECT_EQ(logger.logged_message(), "Error log");

  logger.LogMessage(kLogLevelAssert, "Assert log");
  EXPECT_EQ(logger.logged_message(), "Assert log");
}

TEST(LoggerTest, FilteringStrict) {
  FakeLogger logger;

  logger.SetLogLevel(kLogLevelAssert);

  logger.LogMessage(kLogLevelVerbose, "Verbose log");
  EXPECT_EQ(logger.logged_message(), "");

  logger.LogMessage(kLogLevelDebug, "Debug log");
  EXPECT_EQ(logger.logged_message(), "");

  logger.LogMessage(kLogLevelInfo, "Info log");
  EXPECT_EQ(logger.logged_message(), "");

  logger.LogMessage(kLogLevelWarning, "Warning log");
  EXPECT_EQ(logger.logged_message(), "");

  logger.LogMessage(kLogLevelError, "Error log");
  EXPECT_EQ(logger.logged_message(), "");

  logger.LogMessage(kLogLevelAssert, "Assert log");
  EXPECT_EQ(logger.logged_message(), "Assert log");
}

TEST(LoggerTest, ChainedLogWithEachFunction) {
  FakeLogger parent_logger;
  Logger child_logger(&parent_logger);

  parent_logger.SetLogLevel(kLogLevelVerbose);
  child_logger.SetLogLevel(kLogLevelVerbose);

  child_logger.LogDebug("LogDebug %i", 1);
  EXPECT_EQ(parent_logger.logged_message_level(), kLogLevelDebug);
  EXPECT_EQ(parent_logger.logged_message(), "LogDebug 1");

  child_logger.LogInfo("LogInfo %i", 2);
  EXPECT_EQ(parent_logger.logged_message_level(), kLogLevelInfo);
  EXPECT_EQ(parent_logger.logged_message(), "LogInfo 2");

  child_logger.LogWarning("LogWarning %i", 3);
  EXPECT_EQ(parent_logger.logged_message_level(), kLogLevelWarning);
  EXPECT_EQ(parent_logger.logged_message(), "LogWarning 3");

  child_logger.LogError("LogError %i", 4);
  EXPECT_EQ(parent_logger.logged_message_level(), kLogLevelError);
  EXPECT_EQ(parent_logger.logged_message(), "LogError 4");

  child_logger.LogAssert("LogAssert %i", 5);
  EXPECT_EQ(parent_logger.logged_message_level(), kLogLevelAssert);
  EXPECT_EQ(parent_logger.logged_message(), "LogAssert 5");

  child_logger.LogMessage(kLogLevelInfo, "LogMessage %i", 6);
  EXPECT_EQ(parent_logger.logged_message_level(), kLogLevelInfo);
  EXPECT_EQ(parent_logger.logged_message(), "LogMessage 6");
}

TEST(LoggerTest, ChainedFilteringSameLevel) {
  FakeLogger parent_logger;
  Logger child_logger(&parent_logger);

  parent_logger.SetLogLevel(kLogLevelInfo);
  child_logger.SetLogLevel(kLogLevelInfo);

  child_logger.LogMessage(kLogLevelVerbose, "Verbose log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelDebug, "Debug log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelInfo, "Info log");
  EXPECT_EQ(parent_logger.logged_message(), "Info log");

  child_logger.LogMessage(kLogLevelWarning, "Warning log");
  EXPECT_EQ(parent_logger.logged_message(), "Warning log");

  child_logger.LogMessage(kLogLevelError, "Error log");
  EXPECT_EQ(parent_logger.logged_message(), "Error log");

  child_logger.LogMessage(kLogLevelAssert, "Assert log");
  EXPECT_EQ(parent_logger.logged_message(), "Assert log");
}

TEST(LoggerTest, ChainedFilteringStricterChildLogger) {
  FakeLogger parent_logger;
  Logger child_logger(&parent_logger);

  parent_logger.SetLogLevel(kLogLevelInfo);
  child_logger.SetLogLevel(kLogLevelError);

  child_logger.LogMessage(kLogLevelVerbose, "Verbose log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelDebug, "Debug log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelInfo, "Info log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelWarning, "Warning log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelError, "Error log");
  EXPECT_EQ(parent_logger.logged_message(), "Error log");

  child_logger.LogMessage(kLogLevelAssert, "Assert log");
  EXPECT_EQ(parent_logger.logged_message(), "Assert log");
}

TEST(LoggerTest, ChainedFilteringMorePermissiveChildLogger) {
  FakeLogger parent_logger;
  Logger child_logger(&parent_logger);

  parent_logger.SetLogLevel(kLogLevelError);
  child_logger.SetLogLevel(kLogLevelInfo);

  child_logger.LogMessage(kLogLevelVerbose, "Verbose log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelDebug, "Debug log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelInfo, "Info log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelWarning, "Warning log");
  EXPECT_EQ(parent_logger.logged_message(), "");

  child_logger.LogMessage(kLogLevelError, "Error log");
  EXPECT_EQ(parent_logger.logged_message(), "Error log");

  child_logger.LogMessage(kLogLevelAssert, "Assert log");
  EXPECT_EQ(parent_logger.logged_message(), "Assert log");
}

}  // namespace
}  // namespace internal
}  // namespace firebase
