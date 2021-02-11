#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_FIRESTORE_INTEGRATION_TEST_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_FIRESTORE_INTEGRATION_TEST_H_

#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/mutex.h"
#include "firestore/src/include/firebase/firestore.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

// The interval between checks for future completion.
const int kCheckIntervalMillis = 100;

// The timeout of waiting for a Future or a listener.
const int kTimeOutMillis = 15000;

FirestoreInternal* CreateTestFirestoreInternal(App* app);
void InitializeFirestore(Firestore* instance);

App* GetApp();
App* GetApp(const char* name);
bool ProcessEvents(int msec);

// Converts a Firestore error code to a human-friendly name. The `error_code`
// argument is expected to be an element from the firebase::firestore::Error
// enum, but this function will gracefully handle the case where it is not.
std::string ToFirestoreErrorCodeName(int error_code);

// Waits for a Future to complete. If a timeout is reached then this method
// returns as if successful; therefore, the caller should verify the status of
// the given Future after this function returns. Returns the number of cycles
// that were left before a timeout would have occurred.
int WaitFor(const FutureBase& future);

template <typename T>
class EventAccumulator;

// An EventListener class for writing tests. This listener counts the number of
// events as well as keeps track of the last result.
template <typename T>
class TestEventListener : public EventListener<T> {
 public:
  explicit TestEventListener(std::string name) : name_(std::move(name)) {}

  ~TestEventListener() override {}

  void OnEvent(const T& value, Error error_code,
               const std::string& error_message) override {
    if (print_debug_info_) {
      std::cout << "TestEventListener got: ";
      if (error_code == Error::kErrorOk) {
        std::cout << &value
                  << " from_cache=" << value.metadata().is_from_cache()
                  << " has_pending_write="
                  << value.metadata().has_pending_writes()
                  << " event_count=" << event_count() << std::endl;
      } else {
        std::cout << "error_code=" << error_code << " error_message=\""
                  << error_message << "\" event_count=" << event_count()
                  << std::endl;
      }
    }

    MutexLock lock(mutex_);
    if (error_code != Error::kErrorOk) {
      std::cerr << "ERROR: EventListener " << name_ << " got " << error_code
                << std::endl;
      if (first_error_code_ == Error::kErrorOk) {
        first_error_code_ = error_code;
        first_error_message_ = error_message;
      }
    }
    last_results_.push_back(value);
  }

  int event_count() const {
    MutexLock lock(mutex_);
    return static_cast<int>(last_results_.size());
  }

  const T& last_result(int i = 0) {
    FIREBASE_ASSERT(i >= 0 && i < last_results_.size());
    MutexLock lock(mutex_);
    return last_results_[last_results_.size() - 1 - i];
  }

  // Hides the STLPort-related quirk that `AddSnapshotListener` has different
  // signatures depending on whether `std::function` is available.
  template <typename U>
  ListenerRegistration AttachTo(
      U* ref, MetadataChanges metadata_changes = MetadataChanges::kExclude) {
#if defined(FIREBASE_USE_STD_FUNCTION)
    return ref->AddSnapshotListener(
        metadata_changes, [this](const T& result, Error error_code,
                                 const std::string& error_message) {
          OnEvent(result, error_code, error_message);
        });
#else
    return ref->AddSnapshotListener(metadata_changes, this);
#endif
  }

  std::string first_error_message() {
    MutexLock lock(mutex_);
    return first_error_message_;
  }

  Error first_error_code() {
    MutexLock lock(mutex_);
    return first_error_code_;
  }

  // Set this to true to print more details for each arrived event for debug.
  void set_print_debug_info(bool value) { print_debug_info_ = value; }

  // Copies events from the internal buffer, starting from `start`, up to but
  // not including `end`.
  std::vector<T> GetEventsInRange(int start, int end) const {
    MutexLock lock(mutex_);
    FIREBASE_ASSERT(start <= end);
    FIREBASE_ASSERT(end <= last_results_.size());
    return std::vector<T>(last_results_.begin() + start,
                          last_results_.begin() + end);
  }

 private:
  mutable Mutex mutex_;

  std::string name_;

  // We may want the last N result. So we store all in a vector in the order
  // they arrived.
  std::vector<T> last_results_;

  // We generally only check to see if there is any error. So we only store the
  // first non-OK error, if any.
  Error first_error_code_ = Error::kErrorOk;
  std::string first_error_message_ = "";

  bool print_debug_info_ = false;
};

// Base class for Firestore integration tests.
// Note it keeps a cached of created Firestore instances, and is thread-unsafe.
class FirestoreIntegrationTest : public testing::Test {
  friend class TransactionTester;

 public:
  FirestoreIntegrationTest();
  FirestoreIntegrationTest(const FirestoreIntegrationTest&) = delete;
  FirestoreIntegrationTest(FirestoreIntegrationTest&&) = delete;

  FirestoreIntegrationTest& operator=(const FirestoreIntegrationTest&) = delete;
  FirestoreIntegrationTest& operator=(FirestoreIntegrationTest&&) = delete;

 protected:
  App* app() { return TestFirestore()->app(); }

  // Returns a Firestore instance for an app with the given name.
  // If this method is invoked again with the same `name`, then the same pointer
  // will be returned. The only exception is if the `Firestore` was removed
  // from the cache by a call to `DeleteFirestore()` or `DeleteApp()` with the
  // `App` of the returned `Firestore`.
  Firestore* TestFirestore(const std::string& name = kDefaultAppName) const;

  // Deletes the given `Firestore` instance, which must have been returned by a
  // previous invocation of `TestFirestore()`. If the given instance was in the
  // cache, then it will be removed from the cache. Note that all `Firestore`
  // instances returned from `TestFirestore()` will be automatically deleted at
  // the end of the test case; therefore, this method is only needed if the test
  // requires that the instance be deleted earlier than that.
  void DeleteFirestore(Firestore* firestore);

  // Deletes the given `App` instance. The given `App` must have been the `App`
  // associated with a `Firestore` instance returned by a previous invocation of
  // `TestFirestore()`. Normally the `App` is deleted at the end of the test, so
  // this method is only needed if the test requires the App to be deleted
  // earlier than that. Any `Firestore` instances that were returned from
  // `TestFirestore()` and were associated with the given `App` will be deleted
  // as if with `DeleteFirestore()`.
  void DeleteApp(App* app);

  // Return a reference to the collection with auto-generated id.
  CollectionReference Collection() const;

  // Return a reference to a collection with the path constructed by appending a
  // unique id to the given name.
  CollectionReference Collection(const std::string& name_prefix) const;

  // Return a reference to the collection with given content.
  CollectionReference Collection(
      const std::map<std::string, MapFieldValue>& docs) const;

  // Return an auto-generated document path under collection "test-collection".
  std::string DocumentPath() const;

  // Return a reference to the document with auto-generated id.
  DocumentReference Document() const;

  // Write to the specified document and wait for the write to complete.
  void WriteDocument(DocumentReference reference,
                     const MapFieldValue& data) const;

  // Write to the specified documents to a collection and wait for completion.
  void WriteDocuments(CollectionReference reference,
                      const std::map<std::string, MapFieldValue>& data) const;

  // Update the specified document and wait for the update to complete.
  template <typename MapType>
  void UpdateDocument(DocumentReference reference, const MapType& data) const {
    Future<void> future = reference.Update(data);
    Await(future);
    EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
    EXPECT_EQ(0, future.error()) << DescribeFailedFuture(future) << std::endl;
  }

  // Read the specified document.
  DocumentSnapshot ReadDocument(const DocumentReference& reference) const;

  // Read documents in the specified collection / query.
  QuerySnapshot ReadDocuments(const Query& reference) const;

  // Delete the specified document.
  void DeleteDocument(DocumentReference reference) const;

  // Convert a QuerySnapshot to the id of each document.
  std::vector<std::string> QuerySnapshotToIds(
      const QuerySnapshot& snapshot) const;

  // Convert a QuerySnapshot to the contents of each document.
  std::vector<MapFieldValue> QuerySnapshotToValues(
      const QuerySnapshot& snapshot) const;

  // TODO(zxu): add a helper function to block on signal.

  // A helper function to block until the future completes.
  template <typename T>
  static const T* Await(const Future<T>& future) {
    int cycles = WaitFor(future);
    EXPECT_GT(cycles, 0) << "Waiting future timed out.";
    if (future.status() == FutureStatus::kFutureStatusComplete) {
      if (future.result() == nullptr) {
        std::cout << "WARNING: " << DescribeFailedFuture(future) << std::endl;
      }
    } else {
      std::cout << "WARNING: Future is not completed." << std::endl;
    }
    return future.result();
  }

  static void Await(const Future<void>& future);

  // A helper function to block until there is at least n event.
  template <typename T>
  static void Await(const TestEventListener<T>& listener, int n = 1) {
    // Instead of getting a clock, we count the cycles instead.
    int cycles = kTimeOutMillis / kCheckIntervalMillis;
    while (listener.event_count() < n && cycles > 0) {
      if (ProcessEvents(kCheckIntervalMillis)) {
        std::cout << "WARNING: app receives an event requesting exit."
                  << std::endl;
        return;
      }
      --cycles;
    }
    EXPECT_GT(cycles, 0) << "Waiting listener timed out.";
  }

  // Fails the current test if the given future did not complete or contained an
  // error. Returns true if the future has failed.
  static bool FailIfUnsuccessful(const char* operation,
                                 const FutureBase& future);

  static std::string DescribeFailedFuture(const FutureBase& future);

  void DisableNetwork() { Await(TestFirestore()->DisableNetwork()); }

  void EnableNetwork() { Await(TestFirestore()->EnableNetwork()); }

  static FirestoreInternal* GetFirestoreInternal(Firestore* firestore) {
    return firestore->internal_;
  }

 private:
  template <typename T>
  friend class EventAccumulator;

  class FirestoreInfo {
   public:
    FirestoreInfo() = default;
    FirestoreInfo(const std::string& name, UniquePtr<Firestore>&& firestore)
        : name_(name), firestore_(Move(firestore)) {}

    const std::string& name() const { return name_; }
    Firestore* firestore() const { return firestore_.get(); }
    bool cached() const { return cached_; }
    void ClearCached() { cached_ = false; }

   private:
    std::string name_;
    UniquePtr<Firestore> firestore_;
    bool cached_ = true;
  };

  // The Firestore and App instance caches.
  // Note that `firestores_` is intentionally ordered *after* `apps_` so that
  // the Firestore pointers will be deleted before the App pointers when this
  // object is destructed.
  mutable std::unordered_map<App*, UniquePtr<App>> apps_;
  mutable std::unordered_map<Firestore*, FirestoreInfo> firestores_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_FIRESTORE_INTEGRATION_TEST_H_
