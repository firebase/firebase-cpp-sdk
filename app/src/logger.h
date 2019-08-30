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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_LOGGER_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_LOGGER_H_

#include <stdarg.h>

#include "app/src/include/firebase/log.h"
#include "app/src/log.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// This is a base class for logger implementations.
class LoggerBase {
 public:
  virtual ~LoggerBase();

  // Set the log level.
  //
  // Implementations of LoggerBase are responsible for tracking the log level.
  virtual void SetLogLevel(LogLevel log_level) = 0;

  // Get the currently set log level.
  //
  // Implementations of LoggerBase are responsible for tracking the log level.
  virtual LogLevel GetLogLevel() const = 0;

  // Log a debug message to the system log.
  void LogDebug(const char* format, ...) const;

  // Log an info message to the system log.
  void LogInfo(const char* format, ...) const;

  // Log a warning to the system log.
  void LogWarning(const char* format, ...) const;

  // Log an error to the system log.
  void LogError(const char* format, ...) const;

  // Log an assert and stop the application.
  void LogAssert(const char* format, ...) const;

  // Log a firebase message via LogMessageV().
  void LogMessage(LogLevel log_level, const char* format, ...) const;

  // Log a firebase message.
  void LogMessageV(LogLevel log_level, const char* format, va_list args) const;

 private:
  // Handles the filtering for any message passed to it. If the log level of the
  // message is greater than or equal to the log level of the logger, the
  // message will be passed along to LogMessageImplV.
  void FilterLogMessageV(LogLevel log_level, const char* format,
                         va_list args) const;

  // LogMessageImplV is responsible for doing the actual logging. It should pass
  // along the format string and arguments to whatever library calls handle
  // displaying logs for the given platform.
  virtual void LogMessageImplV(LogLevel log_level, const char* format,
                               va_list args) const = 0;
};

// A logger that calls through to the system logger.
class SystemLogger : public LoggerBase {
 public:
  ~SystemLogger() override;

  void SetLogLevel(LogLevel log_level) override;

  LogLevel GetLogLevel() const override;

 private:
  // Logs a message to the system logger.
  //
  // In Firebase we already have a whole set of wrappers around system level
  // logging, so this merely calls through to ::firebase::LogMessageV.
  void LogMessageImplV(LogLevel log_level, const char* format,
                       va_list args) const override;
};

// Logger is a general logger class that can be chained off of other loggers. It
// does not actually handle displaying the logs themselves, but rather passes
// the message and arguments up to its parent if the message is not filtered.
// Filtering follows the standard rules: the log level of the message must be at
// least as high as that of the current logger being used.  This is useful for
// if you want to have finer grained control over subsystems.
class Logger : public LoggerBase {
 public:
  explicit Logger(const LoggerBase* parent_logger)
      : parent_logger_(parent_logger), log_level_(kDefaultLogLevel) {}
  Logger(const LoggerBase* parent_logger, LogLevel log_level)
      : parent_logger_(parent_logger), log_level_(log_level) {}

  ~Logger() override;

  void SetLogLevel(LogLevel log_level) override;

  LogLevel GetLogLevel() const override;

 private:
  // Passes messages to the parent logger to be displayed.
  void LogMessageImplV(LogLevel log_level, const char* format,
                       va_list args) const override;

  const LoggerBase* parent_logger_;
  LogLevel log_level_;
};

}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_LOGGER_H_
