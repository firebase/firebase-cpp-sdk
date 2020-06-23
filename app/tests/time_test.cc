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

#include "app/src/time.h"

#include <time.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace {

#ifndef WIN32
// Test that the normalize function works, for timespecs
TEST(TimeTests, NormalizeTest) {
  timespec t;
  t.tv_sec = 2;
  t.tv_nsec = firebase::internal::kNanosecondsPerSecond * 5.5;
  firebase::internal::NormalizeTimespec(&t);

  EXPECT_EQ(t.tv_sec, 7);
  EXPECT_EQ(t.tv_nsec, firebase::internal::kNanosecondsPerSecond * 0.5);
}

// Test the various conversions to and from timespecs.
TEST(TimeTests, ConversionTests) {
  timespec t;

  // Test that we can convert timespecs into milliseconds.
  t.tv_sec = 2;
  t.tv_nsec = firebase::internal::kNanosecondsPerSecond * 0.5;
  EXPECT_EQ(firebase::internal::TimespecToMs(t), 2500);

  // Test conversion of milliseconds into timespecs.
  t = firebase::internal::MsToTimespec(6789);
  EXPECT_EQ(t.tv_sec, 6);
  EXPECT_EQ(t.tv_nsec, 789 * firebase::internal::kNanosecondsPerMillisecond);
}

// Test the timespec compare function.
TEST(TimeTests, ComparisonTests) {
  timespec t1, t2;
  clock_gettime(CLOCK_REALTIME, &t1);
  firebase::internal::Sleep(500);
  clock_gettime(CLOCK_REALTIME, &t2);

  EXPECT_EQ(firebase::internal::TimespecCmp(t1, t2), -1);
  EXPECT_EQ(firebase::internal::TimespecCmp(t2, t1), 1);
  EXPECT_EQ(firebase::internal::TimespecCmp(t1, t1), 0);
  EXPECT_EQ(firebase::internal::TimespecCmp(t2, t2), 0);
}
#endif

// Test GetTimestamp function
TEST(TimeTests, GetTimestampTest) {
  uint64_t start = firebase::internal::GetTimestamp();

  firebase::internal::Sleep(500);

  uint64_t end = firebase::internal::GetTimestamp();

  int64_t error = llabs(static_cast<int64_t>(end - start) - 500);

  EXPECT_TRUE(error < 0.10 * firebase::internal::kMillisecondsPerSecond);
}

// Test GetTimestampEpoch function
TEST(TimeTests, GetTimestampEpochTest) {
  uint64_t start = firebase::internal::GetTimestampEpoch();

  firebase::internal::Sleep(500);

  uint64_t end = firebase::internal::GetTimestampEpoch();

  int64_t error = llabs(static_cast<int64_t>(end - start) - 500);

  // Print out the epoch time so that we can verify the timestamp from the log
  // This is the easiest way to verify if the function works in all platform
#ifdef __linux__
  printf("%lu -> %lu (%ld)\n", start, end, error);
#else
  printf("%llu -> %llu (%lld)\n", start, end, error);
#endif  // __linux__

  EXPECT_TRUE(error < 0.10 * firebase::internal::kMillisecondsPerSecond);
}

}  // namespace
