/*
 * Copyright 2018 Google
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

/**
 * Note: this file has a self-contained implementation of `Timestamp` that can
 * be used by the Android library. The resulting Android binary doesn't include
 * symbols from Firestore in third_party, so this implementation has to fill in.
 */

// TODO(b/138615769): remove this file.

#include <ostream>
#include <sstream>

#include "firebase/firestore/timestamp.h"

#if defined(__APPLE__)
#import <CoreFoundation/CoreFoundation.h>
#elif defined(_STLPORT_VERSION)
#include <ctime>
#endif

#include "app/src/assert.h"

namespace firebase {

namespace {

constexpr int32_t kNanosPerSecond = 1E9;

/**
 * Creates a `Timestamp` from the given non-normalized inputs.
 *
 * Timestamp protos require `Timestamp` to always has a positive number of
 * nanoseconds that is counting forward. For negative time, we need to adjust
 * representations with negative nanoseconds. That is, make (negative seconds s1
 * + negative nanoseconds ns1) into (negative seconds s2 + positive nanoseconds
 * ns2). Since nanosecond part is always less than 1 second in our
 * representation, instead of starting at s1 and going back ns1 nanoseconds,
 * start at (s1 minus one second) and go *forward* ns2 = (1 second + ns1, ns1 <
 * 0) nanoseconds.
 */
Timestamp MakeNormalizedTimestamp(int64_t seconds, int64_t nanos) {
  if (nanos < 0) {
    // Note: if nanoseconds are negative, it must mean that seconds are
    // non-positive, but the formula would still be valid, so no need to check.
    seconds = seconds - 1;
    nanos = kNanosPerSecond + nanos;
  }

  FIREBASE_ASSERT(nanos < kNanosPerSecond);

  return {seconds, static_cast<int32_t>(nanos)};
}

}  // namespace

Timestamp::Timestamp(const int64_t seconds, const int32_t nanoseconds)
    : seconds_(seconds), nanoseconds_(nanoseconds) {
  ValidateBounds();
}

Timestamp Timestamp::Now() {
#if defined(__APPLE__)
  // Originally, FIRTimestamp used NSDate to get current time. This method
  // preserves the lower accuracy of that method.
  CFAbsoluteTime now =
      CFAbsoluteTimeGetCurrent() + kCFAbsoluteTimeIntervalSince1970;
  double seconds_double;
  double fraction = modf(now, &seconds_double);
  auto seconds = static_cast<int64_t>(seconds_double);
  auto nanos = static_cast<int32_t>(fraction * kNanosPerSecond);
  return MakeNormalizedTimestamp(seconds, nanos);

#elif !defined(_STLPORT_VERSION)
  // Use the standard <chrono> library from C++11 if possible.
  return FromTimePoint(std::chrono::system_clock::now());
#else
  // If <chrono> is unavailable, use clock_gettime from POSIX, which supports
  // up to nanosecond resolution. Note that it's a non-standard function
  // contained in <time.h>.
  //
  // Note: it's possible to check for availability of POSIX clock_gettime using
  // macros (see "Availability" at https://linux.die.net/man/3/clock_gettime).
  // However, the only platform where <chrono> isn't available is Android with
  // STLPort standard library, where clock_gettime is known to be available.
  timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  return MakeNormalizedTimestamp(now.tv_sec, now.tv_nsec);
#endif  // !defined(_STLPORT_VERSION)
}

Timestamp Timestamp::FromTimeT(const time_t seconds_since_unix_epoch) {
  return {seconds_since_unix_epoch, 0};
}

#if !defined(_STLPORT_VERSION)
Timestamp Timestamp::FromTimePoint(
    const std::chrono::time_point<std::chrono::system_clock> time_point) {
  namespace chr = std::chrono;
  const auto epoch_time = time_point.time_since_epoch();
  auto seconds = chr::duration_cast<chr::duration<int64_t>>(epoch_time);
  auto nanos = chr::duration_cast<chr::nanoseconds>(epoch_time - seconds);

  Timestamp result = MakeNormalizedTimestamp(seconds.count(), nanos.count());
  result.ValidateBounds();
  return result;
}

#endif  // !defined(_STLPORT_VERSION)

std::string Timestamp::ToString() const {
  std::ostringstream stream;
  stream << "Timestamp(seconds=" << seconds_ << ", nanoseconds=" << nanoseconds_
         << ")";
  return stream.str();
}

std::ostream& operator<<(std::ostream& out, const Timestamp& timestamp) {
  return out << timestamp.ToString();
}

void Timestamp::ValidateBounds() const {
  FIREBASE_ASSERT_MESSAGE(nanoseconds_ >= 0,
                          "Timestamp nanoseconds out of range: %d",
                          nanoseconds_);
  FIREBASE_ASSERT_MESSAGE(nanoseconds_ < kNanosPerSecond,
                          "Timestamp nanoseconds out of range: %d",
                          nanoseconds_);
  // Midnight at the beginning of 1/1/1 is the earliest timestamp Firestore
  // supports.
  FIREBASE_ASSERT_MESSAGE(seconds_ >= -62135596800L,
                          "Timestamp seconds out of range: %ld", seconds_);
  // This will break in the year 10,000.
  FIREBASE_ASSERT_MESSAGE(seconds_ < 253402300800L,
                          "Timestamp seconds out of range: %ld", seconds_);
}

}  // namespace firebase
