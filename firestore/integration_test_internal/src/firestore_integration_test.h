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

#ifndef FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_FIRESTORE_INTEGRATION_TEST_H_
#define FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_FIRESTORE_INTEGRATION_TEST_H_

#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app_framework.h"
#include "firestore/src/include/firebase/firestore.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using ::app_framework::LogDebug;
using ::app_framework::LogError;
using ::app_framework::LogInfo;
using ::app_framework::LogWarning;

// The interval between checks for future completion.
const int kCheckIntervalMillis = 100;

// The timeout of waiting for a Future or a listener.
const int kTimeOutMillis = 15000;

FirestoreInternal* CreateTestFirestoreInternal(App* app);

App* GetApp();
App* GetApp(const char* name, const std::string& override_project_id);
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
class TestEventListener {
 public:
  explicit TestEventListener(std::string name) : name_(std::move(name)) {}

  virtual ~TestEventListener() {}

  virtual void OnEvent(const T& value,
                       Error error_code,
                       const std::string& error_message) {
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
    EXPECT_FALSE(fail_on_next_event_);
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

  void FailOnNextEvent() {
    MutexLock lock(mutex_);
    fail_on_next_event_ = true;
  }

  const T& last_result(int i = 0) {
    FIREBASE_ASSERT(i >= 0 && i < last_results_.size());
    MutexLock lock(mutex_);
    return last_results_[last_results_.size() - 1 - i];
  }

  template <typename U>
  ListenerRegistration AttachTo(
      U* ref, MetadataChanges metadata_changes = MetadataChanges::kExclude) {
    return ref->AddSnapshotListener(
        metadata_changes, [this](const T& result, Error error_code,
                                 const std::string& error_message) {
          OnEvent(result, error_code, error_message);
        });
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
  bool fail_on_next_event_ = false;

  // We generally only check to see if there is any error. So we only store the
  // first non-OK error, if any.
  Error first_error_code_ = Error::kErrorOk;
  std::string first_error_message_ = "";

  bool print_debug_info_ = false;
};

// A stopwatch that can calculate the runtime of some operation.
//
// The motivating use case for this class is to include the elapsed time of
// an operation that timed out in the timeout error message.
class Stopwatch {
 public:
  Stopwatch() : start_time_(std::chrono::steady_clock::now()) {}

  std::chrono::duration<double> elapsed_time() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto t =
        stop_time_valid_ ? stop_time_ : std::chrono::steady_clock::now();
    return t - start_time_;
  }

  void stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    assert(!stop_time_valid_);
    stop_time_ = std::chrono::steady_clock::now();
    stop_time_valid_ = true;
  }

 private:
  mutable std::mutex mutex_;
  decltype(std::chrono::steady_clock::now()) start_time_;
  decltype(std::chrono::steady_clock::now()) stop_time_;
  bool stop_time_valid_ = false;
};

std::ostream& operator<<(std::ostream&, const Stopwatch&);

// A RAII wrapper that enables Firestore debug logging and then disables it
// upon destruction.
//
// This is useful for enabling Firestore debug logging in a specific test.
//
// Example:
// TEST(MyCoolTest, VerifyFirestoreDoesItsThing) {
//   FirestoreDebugLogEnabler firestore_debug_log_enabler;
//   ...
// }
class FirestoreDebugLogEnabler {
 public:
  FirestoreDebugLogEnabler() {
    Firestore::set_log_level(LogLevel::kLogLevelDebug);
  }

  ~FirestoreDebugLogEnabler() {
    Firestore::set_log_level(LogLevel::kLogLevelInfo);
  }
};

// Base class for Firestore integration tests.
// Note it keeps a cache of created Firestore instances, and is thread-unsafe.
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
  // from the cache by a call to `DeleteFirestore()` or `DisownFirestore()`, or
  // if `DeleteApp()` is called with the `App` of the returned `Firestore`.
  Firestore* TestFirestore(const std::string& name = kDefaultAppName) const;

  // Returns a Firestore instance for an app with the given `name`, associated
  // with the database with the given `project_id`.
  Firestore* TestFirestoreWithProjectId(const std::string& name,
                                        const std::string& project_id) const;

  // Deletes the given `Firestore` instance, which must have been returned by a
  // previous invocation of `TestFirestore()`, and removes it from the cache of
  // instances returned from `TestFirestore()`. Note that all `Firestore`
  // instances returned from `TestFirestore()` will be automatically deleted at
  // the end of the test case; therefore, this method is only needed if the test
  // requires that the instance be deleted earlier than that. If the given
  // `Firestore` instance has already been removed from the cache, such as by a
  // previous invocation of this method, then the behavior of this method is
  // undefined.
  void DeleteFirestore(Firestore* firestore);

  // Relinquishes ownership of the given `Firestore` instance, which must have
  // been returned by a previous invocation of `TestFirestore()`, and removes it
  // from the cache of instances returned from `TestFirestore()`. Note that all
  // `Firestore` instances returned from `TestFirestore()` will be automatically
  // deleted at the end of the test case; therefore, this method is only needed
  // if the test requires that the instance be excluded from this automatic
  // deletion. If the given `Firestore` instance has already been removed from
  // the cache, such as by a previous invocation of this method, then the
  // behavior of this method is undefined.
  void DisownFirestore(Firestore* firestore);

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

  // Returns a reference to a document with auto-generated id. Writes the given
  // data to the document and waits for the write to complete.
  DocumentReference DocumentWithData(const MapFieldValue& data) const;

  // Write to the specified document and wait for the write to complete.
  void WriteDocument(DocumentReference reference,
                     const MapFieldValue& data) const;

  // Write to the specified documents to a collection and wait for completion.
  void WriteDocuments(CollectionReference reference,
                      const std::map<std::string, MapFieldValue>& data) const;

  // Update the specified document and wait for the update to complete.
  template <typename MapType>
  void UpdateDocument(DocumentReference reference, const MapType& data) const {
    SCOPED_TRACE("FirestoreIntegrationTest::UpdateDocument(" +
                 reference.path() + ")");
    Future<void> future = reference.Update(data);
    Await(future);
    EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
    EXPECT_EQ(0, future.error()) << DescribeFailedFuture(future) << std::endl;
  }

  // Read the specified document.
  DocumentSnapshot ReadDocument(const DocumentReference& reference) const;

  // Read documents in the specified collection / query.
  QuerySnapshot ReadDocuments(const Query& reference) const;

  // Read the aggregate.
  AggregateQuerySnapshot ReadAggregate(
      const AggregateQuery& aggregate_query) const;

  // Delete the specified document.
  void DeleteDocument(DocumentReference reference) const;

  // Convert a QuerySnapshot to the id of each document.
  std::vector<std::string> QuerySnapshotToIds(
      const QuerySnapshot& snapshot) const;

  // Convert a QuerySnapshot to the contents of each document.
  std::vector<MapFieldValue> QuerySnapshotToValues(
      const QuerySnapshot& snapshot) const;

  // Convert a QuerySnapshot to a map from document id to document content.
  std::map<std::string, MapFieldValue> QuerySnapshotToMap(
      const QuerySnapshot& snapshot) const;

  // TODO(zxu): add a helper function to block on signal.

  // A helper function to block until the future completes.
  template <typename T>
  static const T* Await(const Future<T>& future) {
    Stopwatch stopwatch;
    int cycles = WaitFor(future);
    EXPECT_GT(cycles, 0) << "Waiting future timed out after " << stopwatch;
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
    Stopwatch stopwatch;
    int cycles = kTimeOutMillis / kCheckIntervalMillis;
    while (listener.event_count() < n && cycles > 0) {
      if (ProcessEvents(kCheckIntervalMillis)) {
        std::cout << "WARNING: app receives an event requesting exit."
                  << std::endl;
        return;
      }
      --cycles;
    }
    EXPECT_GT(cycles, 0) << "Waiting listener timed out after " << stopwatch;
  }

  // Fails the current test if the given future did not complete or contained an
  // error. Returns true if the future has failed. The given Stopwatch will be
  // used to include the elapsed time in any failure message.
  static bool FailIfUnsuccessful(const char* operation,
                                 const FutureBase& future,
                                 const Stopwatch& stopwatch);

  static std::string DescribeFailedFuture(const FutureBase& future);

  void DisableNetwork() {
    SCOPED_TRACE("FirestoreIntegrationTest::DisableNetwork()");
    Await(TestFirestore()->DisableNetwork());
  }

  void EnableNetwork() {
    SCOPED_TRACE("FirestoreIntegrationTest::EnableNetwork()");
    Await(TestFirestore()->EnableNetwork());
  }

  static FirestoreInternal* GetFirestoreInternal(Firestore* firestore) {
    return firestore->internal_;
  }

 private:
  template <typename T>
  friend class EventAccumulator;

  class FirestoreInfo {
   public:
    FirestoreInfo() = default;
    FirestoreInfo(const std::string& name, std::unique_ptr<Firestore> firestore)
        : name_(name), firestore_(std::move(firestore)) {}

    const std::string& name() const { return name_; }
    Firestore* firestore() const { return firestore_.get(); }
    void ReleaseFirestore() { firestore_.release(); }

   private:
    std::string name_;
    std::unique_ptr<Firestore> firestore_;
  };

  // The Firestore and App instance caches.
  // Note that `firestores_` is intentionally ordered *after* `apps_` so that
  // the Firestore pointers will be deleted before the App pointers when this
  // object is destructed.
  mutable std::unordered_map<App*, std::unique_ptr<App>> apps_;
  mutable std::unordered_map<Firestore*, FirestoreInfo> firestores_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_FIRESTORE_INTEGRATION_TEST_H_
