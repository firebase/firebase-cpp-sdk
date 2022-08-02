/*
 * Copyright 2021 Google LLC
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

#ifndef FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_EVENT_ACCUMULATOR_H_
#define FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_EVENT_ACCUMULATOR_H_

#include <vector>

#include "firestore_integration_test.h"

namespace firebase {
namespace firestore {

// Event accumulator for integration test. Ported from the native SDKs.
template <typename T>
class EventAccumulator {
 public:
  EventAccumulator() : listener_("EventAccumulator") {}

  TestEventListener<T>* listener() { return &listener_; }

  std::vector<T> Await(int num_events) {
    SCOPED_TRACE("EventAccumulator::Await() num_events=" +
                 std::to_string(num_events));
    int old_num_events = num_events_consumed_;
    int desired_events = num_events_consumed_ + num_events;
    FirestoreIntegrationTest::Await(listener_, desired_events);

    if (listener_.first_error_code() != Error::kErrorOk ||
        listener_.event_count() < desired_events) {
      int received = listener_.event_count() - num_events_consumed_;
      ADD_FAILURE() << "Failed to await " << num_events
                    << " events: error_code=" << listener_.first_error_code()
                    << " error_message=\"" << listener_.first_error_message()
                    << "\", received " << received << " events";

      // If there are fewer events than requested, discard them.
      num_events_consumed_ += received;
      return {};
    }

    num_events_consumed_ = desired_events;
    return listener_.GetEventsInRange(old_num_events, num_events_consumed_);
  }

  /** Awaits 1 event. */
  T Await() {
    SCOPED_TRACE("EventAccumulator::Await()");
    auto events = Await(1);
    if (events.empty()) {
      return {};
    } else {
      return events[0];
    }
  }

  /** Waits for a snapshot with pending writes. */
  T AwaitLocalEvent() {
    SCOPED_TRACE("EventAccumulator::AwaitLocalEvent()");
    T event;
    do {
      event = Await();
    } while (!HasPendingWrites(event));
    return event;
  }

  /** Waits for a snapshot that has no pending writes. */
  T AwaitRemoteEvent() {
    SCOPED_TRACE("EventAccumulator::AwaitRemoteEvent()");
    T event;
    do {
      event = Await();
    } while (HasPendingWrites(event));
    return event;
  }

  /**
   * Waits for a snapshot that is from cache.
   * NOTE: Helper exists only in C++ test. Not in native SDK test yet.
   */
  T AwaitCacheEvent() {
    SCOPED_TRACE("EventAccumulator::AwaitCacheEvent()");
    T event;
    do {
      event = Await();
    } while (!IsFromCache(event));
    return event;
  }

  /**
   * Waits for a snapshot that is not from cache.
   * NOTE: Helper exists only in C++ test. Not in native SDK test yet.
   */
  T AwaitServerEvent() {
    SCOPED_TRACE("EventAccumulator::AwaitServerEvent()");
    T event;
    do {
      event = Await();
    } while (IsFromCache(event));
    return event;
  }

  void FailOnNextEvent() { listener_.FailOnNextEvent(); }

 private:
  bool HasPendingWrites(T event) {
    return event.metadata().has_pending_writes();
  }

  bool IsFromCache(T event) { return event.metadata().is_from_cache(); }

  TestEventListener<T> listener_;

  // Total events consumed by callers of EventAccumulator. This differs from
  // listener_.event_count() because that represents the number of events
  // available, whereas this represents the number actually consumed. These can
  // diverge if events arrive more rapidly than the tests consume them.
  int num_events_consumed_ = 0;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_EVENT_ACCUMULATOR_H_
