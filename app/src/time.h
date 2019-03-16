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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_TIME_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_TIME_H_
#include <cstdint>
#include <cassert>

#ifdef WIN32
#include <windows.h>
#else  // WIN32
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#endif  // WIN32

#ifdef __MACH__
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace internal {

// Some handy conversion constants.
const int64_t kSecondsPerMinute = 60;
const int64_t kMinutesPerHour = 60;
const int64_t kMillisecondsPerSecond = 1000;
const int64_t kMicrosecondsPerMillisecond = 1000;
const int64_t kMillisecondsPerMinute =
    kMillisecondsPerSecond * kSecondsPerMinute;
const int64_t kNanosecondsPerSecond = 1000000000;
const int64_t kNanosecondsPerMillisecond = 1000000;

// Platform agnostic sleep function, for situations where need to just have
// the current thread stop and wait for a bit.
inline void Sleep(int64_t milliseconds) {
#ifdef WIN32
  // win32 sleep accepts milliseconds, which seems reasonable
  const int64_t max_time = static_cast<int64_t>(~static_cast<DWORD>(0));
  if (milliseconds > max_time) milliseconds = max_time;
  ::Sleep(static_cast<DWORD>(milliseconds));
#else   // WIN32
  // Unix/Linux/Mac usleep accepts microseconds.
  usleep(milliseconds * kMicrosecondsPerMillisecond);
#endif  // WIN32
}

#ifndef WIN32
// Utility function for normalizing a timespec.
inline void NormalizeTimespec(timespec* t) {
  t->tv_sec += t->tv_nsec / kNanosecondsPerSecond;
  t->tv_nsec %= kNanosecondsPerSecond;
}

// Utility function, for converting a timespec struct into milliseconds.
inline int64_t TimespecToMs(timespec tm) {
  return tm.tv_sec * FIREBASE_NAMESPACE::internal::kMillisecondsPerSecond +
         tm.tv_nsec / FIREBASE_NAMESPACE::internal::kNanosecondsPerMillisecond;
}

// Utility function for converting milliseconds into a timespec structure.
inline timespec MsToTimespec(int milliseconds) {
  timespec t;
  t.tv_sec = milliseconds / internal::kMillisecondsPerSecond;
  t.tv_nsec = (milliseconds - t.tv_sec * internal::kMillisecondsPerSecond) *
              internal::kNanosecondsPerMillisecond;
  return t;
}

// Utility function for converting milliseconds into a timespec structure
// describing the absolute calendar time, [milliseconds] in the future.
inline timespec MsToAbsoluteTimespec(int milliseconds) {
  timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  t.tv_nsec += milliseconds * internal::kNanosecondsPerMillisecond;
  NormalizeTimespec(&t);
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
#endif  // ifndef _WIN32

// Return a timestamp in milliseconds since a starting time which varies for
// different platform.
// This is a light-weight function best suitable to calculate elapse time
// locally.
inline uint64_t GetTimestamp() {
#if defined(WIN32)
  // return the elapse time since the system is started in milliseconds
  return GetTickCount64();
#elif defined(__MACH__)
  // clock_gettime is only supported on macOS after 10.10 Sierra.
  // mach_absolute_time() returns absolute time in nano seconds.
#ifdef TARGET_OS_IOS
  // Note that the same function does NOT return nano seconds in iOS.  Requires
  // mach_timebase_info_data_t to convert returned value into nano seconds.
  // However, the conversion may have potential risk to introduce overflow or
  // error.  Re-evaluate this when we need to use this function in iOS.
  assert(false);
#endif
  return mach_absolute_time() / internal::kNanosecondsPerMillisecond;
#else
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * internal::kMillisecondsPerSecond +
         t.tv_nsec / internal::kNanosecondsPerMillisecond;
#endif  // ifdef WIN32
}

// Return a timestamp in milliseconds since Epoch.
// This is used to communicate with Firebase server which uses epoch time in ms.
inline uint64_t GetTimestampEpoch() {
#if defined(WIN32)
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
  return t.tv_sec * internal::kMillisecondsPerSecond +
         t.tv_nsec / internal::kNanosecondsPerMillisecond;
#endif  // ifdef WIN32
}

// High resolution timer.
// Copied from https://github.com/google/mathfu/blob/master/benchmarks/benchmark_common.h
class Timer {
 public:
  Timer() {
    InitializeTickPeriod();
    Reset();
  }

  // Save the current number of counter ticks.
  void Reset() {
    start_ = GetTicks();
  }

  // Get the time elapsed in counter ticks since Reset() was called.
  uint64_t GetElapsedTicks() {
    return GetTicks() - start_;
  }

  // Get the time elapsed in seconds since Reset() was called.
  double GetElapsedSeconds() {
    return static_cast<double>(GetElapsedTicks()) * tick_period_;
  }

 public:
  // Initialize the tick period value.
  static void InitializeTickPeriod() {
    if (tick_period_ != 0) {
      return;
    }
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    tick_period_ = 1.0 / static_cast<double>(frequency.QuadPart);
#elif defined(__linux__)
    // Use a fixed frequency of 1ns to match timespec.
    tick_period_ = 1e-9;
#else
    // Use a fixed frequency of 1us to match timeval.
    tick_period_ = 1e-6;
#endif  // defined(_WIN32)
  }

  // Get the period of one counter tick.
  static double GetTickPeriod() {
    InitializeTickPeriod();
    return tick_period_;
  }

  // Get the number of counter ticks elapsed.
  static uint64_t GetTicks() {
#if defined(_WIN32)
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
#elif defined(__linux__)
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (static_cast<uint64_t>(time.tv_sec) * 1000000000ULL) +
        time.tv_nsec;
#else
    struct timeval time;
    gettimeofday(&time, NULL);
    return (static_cast<uint64_t>(time.tv_sec) * 1000000ULL) +
        time.tv_usec;
#endif  // defined(_WIN32)
  }

 private:
  uint64_t start_;
  static double tick_period_;
};
}  // namespace internal
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_TIME_H_
