// Copyright 2018 Google LLC

#include <cinttypes>
#include <cstdint>
#include <ostream>
#include <sstream>

#include "app/src/assert.h"
#include "firebase/firestore/timestamp.h"

namespace firebase {
namespace {

constexpr int32_t kNanosPerSecond = 1E9;

Timestamp MakeNormalizedTimestamp(int64_t seconds, int64_t nanos) {
  if (nanos < 0) {
    seconds -= 1;
    nanos += kNanosPerSecond;
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
  return FromTimePoint(std::chrono::system_clock::now());
}

Timestamp Timestamp::FromTimeT(const time_t seconds_since_unix_epoch) {
  return {seconds_since_unix_epoch, 0};
}

Timestamp Timestamp::FromTimePoint(
    const std::chrono::time_point<std::chrono::system_clock> time_point) {
  namespace chr = std::chrono;
  const auto epoch_time = time_point.time_since_epoch();
  auto seconds = chr::duration_cast<chr::duration<int64_t>>(epoch_time);
  auto nanos = chr::duration_cast<chr::nanoseconds>(epoch_time - seconds);
  return MakeNormalizedTimestamp(seconds.count(), nanos.count());
}

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
  FIREBASE_ASSERT_MESSAGE(nanoseconds_ >= 0 && nanoseconds_ < kNanosPerSecond,
                          "Timestamp nanoseconds out of range: %d",
                          nanoseconds_);
  FIREBASE_ASSERT_MESSAGE(
      seconds_ >= -62135596800LL && seconds_ < 253402300800LL,
      "Timestamp seconds out of range: %" PRId64, seconds_);
}

}  // namespace firebase
