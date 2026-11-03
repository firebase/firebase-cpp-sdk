/*
 * Copyright 2018 Google LLC
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

#ifndef FIREBASE_APP_SRC_TIME_H_
#define FIREBASE_APP_SRC_TIME_H_
#include <cerrno>
#include <cstdint>

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <windows.h>
#else  // !FIREBASE_PLATFORM_WINDOWS
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif  // FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS ||
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace internal {

// Some handy conversion constants.
constexpr int64_t kSecondsPerMinute = 60;
constexpr int64_t kMinutesPerHour = 60;
constexpr int64_t kMillisecondsPerSecond = 1000;
constexpr int64_t kMicrosecondsPerMillisecond = 1000;
constexpr int64_t kMillisecondsPerMinute =
    kMillisecondsPerSecond * kSecondsPerMinute;
constexpr int64_t kNanosecondsPerSecond = 1000000000;
constexpr int64_t kNanosecondsPerMillisecond = 1000000;

// Platform agnostic sleep function, for situations where need to just have
// the current thread stop and wait for a bit.
inline void Sleep(int64_t milliseconds) {
  if (milliseconds <= 0) {
    return;
  }
#if FIREBASE_PLATFORM_WINDOWS
  // Win32 Sleep accepts DWORD milliseconds, where 0xFFFFFFFF is INFINITE.
  // Clamp each sleep call to INFINITE - 1 (0xFFFFFFFE) in a loop.
  constexpr int64_t kMaxSleepTimeMs = static_cast<int64_t>(INFINITE - 1);
  while (milliseconds > 0) {
    const int64_t sleep_ms =
        milliseconds > kMaxSleepTimeMs ? kMaxSleepTimeMs : milliseconds;
    ::Sleep(static_cast<DWORD>(sleep_ms));
    milliseconds -= sleep_ms;
  }
#else   // !FIREBASE_PLATFORM_WINDOWS
  timespec req;
  req.tv_sec =
      static_cast<time_t>(milliseconds / internal::kMillisecondsPerSecond);
  req.tv_nsec = static_cast<long>(
      (milliseconds % internal::kMillisecondsPerSecond) *
      internal::kNanosecondsPerMillisecond);
  timespec rem;
  while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
    req = rem;
  }
#endif  // FIREBASE_PLATFORM_WINDOWS
}

#if !FIREBASE_PLATFORM_WINDOWS
// Utility function, for converting a timespec struct into milliseconds.
inline int64_t TimespecToMs(timespec tm) {
  return static_cast<int64_t>(tm.tv_sec) *
             firebase::internal::kMillisecondsPerSecond +
         tm.tv_nsec / firebase::internal::kNanosecondsPerMillisecond;
}

// Utility function for converting milliseconds into a timespec structure.
inline timespec MsToTimespec(int64_t milliseconds) {
  timespec t;
  t.tv_sec = static_cast<time_t>(milliseconds / internal::kMillisecondsPerSecond);
  t.tv_nsec = static_cast<long>(
      (milliseconds % internal::kMillisecondsPerSecond) *
      internal::kNanosecondsPerMillisecond);
  if (t.tv_nsec < 0) {
    t.tv_sec -= 1;
    t.tv_nsec += static_cast<long>(internal::kNanosecondsPerSecond);
  }
  return t;
}

// Utility function for converting milliseconds into a timespec structure
// describing the absolute calendar time, [milliseconds] in the future.
inline timespec MsToAbsoluteTimespec(int64_t milliseconds) {
  timespec t;
  clock_gettime(CLOCK_REALTIME, &t);

  const int64_t nanoseconds =
      static_cast<int64_t>(t.tv_nsec) +
      (static_cast<int64_t>(t.tv_sec) * internal::kNanosecondsPerSecond) +
      (milliseconds * internal::kNanosecondsPerMillisecond);

  t.tv_sec = static_cast<time_t>(nanoseconds / internal::kNanosecondsPerSecond);
  t.tv_nsec = static_cast<long>(nanoseconds % internal::kNanosecondsPerSecond);
  if (t.tv_nsec < 0) {
    t.tv_sec -= 1;
    t.tv_nsec += static_cast<long>(internal::kNanosecondsPerSecond);
  }
  return t;
}

// Compares two timespecs.  Returns -1 if the first is less than the second,
// returns 1 if the second is less than the first, and 0 if they are equal.
inline int64_t TimespecCmp(const timespec& t1, const timespec& t2) {
  if (t1.tv_sec < t2.tv_sec) {
    return -1;
  } else if (t1.tv_sec > t2.tv_sec) {
    return 1;
  } else {  // seconds are equal, check nanoseconds.
    if (t1.tv_nsec < t2.tv_nsec) {
      return -1;
    } else if (t1.tv_nsec > t2.tv_nsec) {
      return 1;
    } else {  // seconds are equal, check nanoseconds.
      return 0;
    }
  }
}
#endif  // !FIREBASE_PLATFORM_WINDOWS

// Return a timestamp in milliseconds since a starting time which varies for
// different platform.
// This is a light-weight function best suitable to calculate elapse time
// locally.
inline uint64_t GetTimestamp() {
#if FIREBASE_PLATFORM_WINDOWS
  // return the elapse time since the system is started in milliseconds
  return GetTickCount64();
#elif FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  // Convert mach_absolute_time() ticks to milliseconds using
  // mach_timebase_info_data_t without 64-bit integer overflow.
  static const mach_timebase_info_data_t timebase_info = [] {
    mach_timebase_info_data_t info = {0, 0};
    mach_timebase_info(&info);
    return info;
  }();
  uint64_t ticks = mach_absolute_time();
  uint64_t numer = timebase_info.numer;
  uint64_t denom =
      static_cast<uint64_t>(timebase_info.denom) *
      static_cast<uint64_t>(internal::kNanosecondsPerMillisecond);
  return (ticks / denom) * numer + ((ticks % denom) * numer) / denom;
#else   // Linux / Android
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return static_cast<uint64_t>(t.tv_sec) *
             static_cast<uint64_t>(internal::kMillisecondsPerSecond) +
         static_cast<uint64_t>(t.tv_nsec) /
             static_cast<uint64_t>(internal::kNanosecondsPerMillisecond);
#endif  // platform select
}

// Return a timestamp in milliseconds since Epoch.
// This is used to communicate with Firebase server which uses epoch time in ms.
inline uint64_t GetTimestampEpoch() {
#if FIREBASE_PLATFORM_WINDOWS
  // Get the current file time, the number of 100-nanosecond intervals since
  // January 1, 1601 (UTC).
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  // convert to a unsigned long long as suggested on MSDN
  ULARGE_INTEGER uli;
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;

  // Convert to milliseconds and remove the offet between year 1601 and 1970
  return uli.QuadPart / 10000ULL - 11644473600000ULL;
#else
  // Both Linux and Android supports clock_gettime(CLOCK_REALTIME)
  // But Android does not support gettimeofday()
  timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return static_cast<uint64_t>(t.tv_sec) *
             static_cast<uint64_t>(internal::kMillisecondsPerSecond) +
         static_cast<uint64_t>(t.tv_nsec) /
             static_cast<uint64_t>(internal::kNanosecondsPerMillisecond);
#endif  // FIREBASE_PLATFORM_WINDOWS
}

// High resolution timer.
// Copied from
// https://github.com/google/mathfu/blob/master/benchmarks/benchmark_common.h
class Timer {
 public:
  Timer() {
    InitializeTickPeriod();
    Reset();
  }

  // Save the current number of counter ticks.
  void Reset() { start_ = GetTicks(); }

  // Get the time elapsed in counter ticks since Reset() was called.
  uint64_t GetElapsedTicks() const { return GetTicks() - start_; }

  // Get the time elapsed in seconds since Reset() was called.
  double GetElapsedSeconds() const {
    return static_cast<double>(GetElapsedTicks()) * GetTickPeriod();
  }

  // Initialize the tick period value.
  static void InitializeTickPeriod() { (void)GetTickPeriod(); }

  // Get the period of one counter tick.
  static double GetTickPeriod() {
    static const double kTickPeriod = []() -> double {
#if FIREBASE_PLATFORM_WINDOWS
      LARGE_INTEGER frequency;
      QueryPerformanceFrequency(&frequency);
      return 1.0 / static_cast<double>(frequency.QuadPart);
#elif FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
      mach_timebase_info_data_t info = {0, 0};
      mach_timebase_info(&info);
      return (static_cast<double>(info.numer) /
              static_cast<double>(info.denom)) *
             1e-9;
#else
      // Use a fixed frequency of 1ns to match timespec.
      return 1e-9;
#endif  // FIREBASE_PLATFORM_WINDOWS
    }();
    return kTickPeriod;
  }

  // Get the number of counter ticks elapsed.
  static uint64_t GetTicks() {
#if FIREBASE_PLATFORM_WINDOWS
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    return static_cast<uint64_t>(ticks.QuadPart);
#elif FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    return mach_absolute_time();
#else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (static_cast<uint64_t>(time.tv_sec) * 1000000000ULL) +
           static_cast<uint64_t>(time.tv_nsec);
#endif  // FIREBASE_PLATFORM_WINDOWS
  }

 private:
  uint64_t start_ = 0;
};
}  // namespace internal
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_TIME_H_
