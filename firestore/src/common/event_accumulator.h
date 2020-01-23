#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_EVENT_ACCUMULATOR_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_EVENT_ACCUMULATOR_H_

#include "firestore/src/common/firestore_integration_test.h"

namespace firebase {
namespace firestore {

// Event accumulator for integration test. Ported from the native SDKs.
template <typename T>
class EventAccumulator {
 public:
  EventAccumulator() : listener_("EventAccumulator") {}

  TestEventListener<T>* listener() { return &listener_; }

  std::vector<T> Await(int num_events) {
    max_events_ += num_events;
    FirestoreIntegrationTest::Await(listener_, max_events_);
    EXPECT_EQ(Error::Ok, listener_.first_error());

    std::vector<T> result;
    // We cannot use listener.last_result() as it goes backward and can
    // contains more than max_events_ events. So we look up manually.
    for (int i = max_events_ - num_events; i < max_events_; ++i) {
      result.push_back(listener_.last_result_[i]);
    }
    return result;
  }

  /** Await 1 event. */
  T Await() { return Await(1)[0]; }

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
  int max_events_ = 0;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_EVENT_ACCUMULATOR_H_
