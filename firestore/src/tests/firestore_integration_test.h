#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_FIRESTORE_INTEGRATION_TEST_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_FIRESTORE_INTEGRATION_TEST_H_

#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

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
  ~FirestoreIntegrationTest() override;

  FirestoreIntegrationTest& operator=(const FirestoreIntegrationTest&) = delete;
  FirestoreIntegrationTest& operator=(FirestoreIntegrationTest&&) = delete;

 protected:
  App* app() { return firestore()->app(); }

  Firestore* firestore() const { return CachedFirestore(kDefaultAppName); }

  // If no Firestore instance is registered under the name, creates a new
  // instance in order to have multiple Firestore clients for testing.
  // Otherwise, returns the registered Firestore instance.
  Firestore* CachedFirestore(const std::string& name) const;

  // Blocks until the Firestore instance corresponding to the given app name
  // shuts down, deletes the instance and removes the pointer to it from the
  // cache. Asserts that a Firestore instance with the given name does exist.
  void DeleteFirestore(const std::string& name = kDefaultAppName);

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

  // Creates a new Firestore instance, without any caching, using a uniquely-
  // generated app_name.
  // Use Release() to correctly delete the returned pointer.
  Firestore* CreateFirestore() const;
  // Creates a new Firestore instance, without any caching, using the given
  // app_name.
  // Use Release() to correctly delete the returned pointer.
  Firestore* CreateFirestore(const std::string& app_name) const;

  void DisableNetwork() { Await(firestore()->DisableNetwork()); }

  void EnableNetwork() { Await(firestore()->EnableNetwork()); }

  // Deletes the given Firestore instance and deletes the app by which it is
  // owned.
  static void Release(Firestore* firestore);

 private:
  template <typename T>
  friend class EventAccumulator;

  // The Firestore instance cache.
  mutable std::map<std::string, Firestore*> firestores_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_FIRESTORE_INTEGRATION_TEST_H_
