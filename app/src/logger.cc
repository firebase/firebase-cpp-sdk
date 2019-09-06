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

#include "app/src/log.h"

namespace FIREBASE_NAMESPACE {

LoggerBase::~LoggerBase() {}

void LoggerBase::LogDebug(const char* format, ...) const {
  va_list list;
  va_start(list, format);
  FilterLogMessageV(kLogLevelDebug, format, list);
  va_end(list);
}

void LoggerBase::LogInfo(const char* format, ...) const {
  va_list list;
  va_start(list, format);
  FilterLogMessageV(kLogLevelInfo, format, list);
  va_end(list);
}

void LoggerBase::LogWarning(const char* format, ...) const {
  va_list list;
  va_start(list, format);
  FilterLogMessageV(kLogLevelWarning, format, list);
  va_end(list);
}

void LoggerBase::LogError(const char* format, ...) const {
  va_list list;
  va_start(list, format);
  FilterLogMessageV(kLogLevelError, format, list);
  va_end(list);
}

void LoggerBase::LogAssert(const char* format, ...) const {
  va_list list;
  va_start(list, format);
  FilterLogMessageV(kLogLevelAssert, format, list);
  va_end(list);
}

void LoggerBase::LogMessage(LogLevel log_level, const char* format, ...) const {
  va_list list;
  va_start(list, format);
  FilterLogMessageV(log_level, format, list);
  va_end(list);
}

void LoggerBase::LogMessageV(LogLevel log_level, const char* format,
                             va_list args) const {
  FilterLogMessageV(log_level, format, args);
}

void LoggerBase::FilterLogMessageV(LogLevel log_level, const char* format,
                                   va_list args) const {
  if (log_level >= this->GetLogLevel()) {
    LogMessageImplV(log_level, format, args);
  }
}

SystemLogger::~SystemLogger() {}

void SystemLogger::SetLogLevel(LogLevel log_level) {
  ::FIREBASE_NAMESPACE::SetLogLevel(log_level);
}

LogLevel SystemLogger::GetLogLevel() const {
  return ::FIREBASE_NAMESPACE::GetLogLevel();
}

void SystemLogger::LogMessageImplV(LogLevel log_level, const char* format,
                                   va_list args) const {
  ::FIREBASE_NAMESPACE::LogMessageWithCallbackV(log_level, format, args);
}

Logger::~Logger() {}

void Logger::SetLogLevel(LogLevel log_level) { log_level_ = log_level; }

LogLevel Logger::GetLogLevel() const { return log_level_; }

void Logger::LogMessageImplV(LogLevel log_level, const char* format,
                             va_list args) const {
  parent_logger_->LogMessageV(log_level, format, args);
}

}  // namespace FIREBASE_NAMESPACE
