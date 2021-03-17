#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_EVENT_ACCUMULATOR_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_EVENT_ACCUMULATOR_H_

#include "firestore/src/tests/firestore_integration_test.h"

namespace firebase {
namespace firestore {

// Event accumulator for integration test. Ported from the native SDKs.
template <typename T>
class EventAccumulator {
 public:
  EventAccumulator() : listener_("EventAccumulator") {}

  TestEventListener<T>* listener() { return &listener_; }

  std::vector<T> Await(int num_events) {
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
    auto events = Await(1);
    if (events.empty()) {
      return {};
    } else {
      return events[0];
    }
  }

  /** Waits for a snapshot with pending writes. */
  T AwaitLocalEvent() {
    T event;
    do {
      event = Await();
    } while (!HasPendingWrites(event));
    return event;
  }

  /** Waits for a snapshot that has no pending writes. */
  T AwaitRemoteEvent() {
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
    T event;
    do {
      event = Await();
    } while (IsFromCache(event));
    return event;
  }

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
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_EVENT_ACCUMULATOR_H_
