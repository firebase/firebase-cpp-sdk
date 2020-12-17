#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TRANSACTION_MANAGER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TRANSACTION_MANAGER_H_

#include <cstdint>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <string>
#include <utility>

#include "app/src/cleanup_notifier.h"
#include "firebase/future.h"
#include "firebase/csharp/map.h"
#include "firebase/firestore.h"
#include "firebase/firestore/document_snapshot.h"
#include "firebase/firestore/firestore_errors.h"

// Add this to make this header compile when SWIG is not involved.
#ifndef SWIGSTDCALL
#if !defined(SWIG) && \
    (defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__))
#define SWIGSTDCALL __stdcall
#else
#define SWIGSTDCALL
#endif
#endif

namespace firebase {
namespace firestore {
namespace csharp {

class TransactionCallback;
class TransactionCallbackInternal;
class TransactionManagerInternal;

// The type of the callback function to be specified to
// `TransactionManager::RunTransaction()`. The `TransactionCallback` argument
// contains all information required for C# to execute the callback. Ownership
// of the `TransactionCallback` is transferred to the function.
//
// Returns `true` if the callback was successful or `false` otherwise. If
// successful, then the `OnCompletion()` must have been invoked or must be
// invoked at some point in the future when the callback completes. If
// unsuccessful, then it is not required that `OnCompletion()` be invoked, and
// the transaction will complete in a failed state.
// NOLINTNEXTLINE(readability/casting)
typedef bool(SWIGSTDCALL* TransactionCallbackFn)(TransactionCallback*);

// Stores the result of calling `Transaction::Get()` in a manner that is
// convenient for exposure to C# via SWIG.
//
// This class is not thread safe.
class TransactionGetResult {
 public:
  // Creates a new "invalid" instance of this class.
  TransactionGetResult() = default;

  // Creates a new "valid" instance of this class with the given information.
  TransactionGetResult(DocumentSnapshot&& snapshot, Error error_code,
                       std::string&& error_message)
      : valid_(true),
        snapshot_(std::move(snapshot)),
        error_code_(error_code),
        error_message_(std::move(error_message)) {}

  // Returns whether or not this object is "valid".
  //
  // If this object is "valid" then the other methods in this class have well-
  // defined return values and behaviors; otherwise, the other methods in this
  // class have undefined return values and behaviors and should not be invoked.
  bool is_valid() const { return valid_; }

  // Returns the `DocumentSnapshot` result of the `Transaction::Get()` call.
  //
  // This method consumes the `DocumentSnapshot` upon its first invocation;
  // subsequent invocations will return an invalid snapshot.
  DocumentSnapshot TakeSnapshot() { return std::move(snapshot_); }

  // Returns the error code result of the `Transaction::Get()` call.
  Error error_code() const { return error_code_; }

  // Returns the error message result of the `Transaction::Get()` call.
  const std::string& error_message() const { return error_message_; }

 private:
  // TODO(b/172570071) Replace `valid_` with `DocumentSnapshot::is_valid()`.
  bool valid_ = false;
  DocumentSnapshot snapshot_;
  Error error_code_ = Error::kErrorUnknown;
  std::string error_message_;
};

// Provides all information and machinery required to perform a transaction
// callback. This includes the callback ID that was specified to
// `RunTransaction()`, an `OnCompletion()` method to signal the success or
// failure of the callback, and indirect access to the `Transaction` object
// associated with the callback.
//
// This class is thread safe.
class TransactionCallback {
 public:
  TransactionCallback(std::shared_ptr<TransactionCallbackInternal> internal,
                      int32_t callback_id, TransactionCallbackFn callback)
      : internal_(internal), callback_id_(callback_id), callback_(callback) {}

  // Returns the internal `shared_ptr` that was specified to
  // the constructor.
  std::shared_ptr<TransactionCallbackInternal> internal() const {
    return internal_;
  }

  // Returns the callback ID that was specified to
  // `TransactionManager::RunTransaction()`.
  int32_t callback_id() const { return callback_id_; }

  // Returns the callback function that was specified to
  // `TransactionManager::RunTransaction()`.
  TransactionCallbackFn callback() const { return callback_; }

  // Calls `Get()` on the encapsulated `Transaction` object with the given
  // arguments. Returns a "valid" result object if the call succeeded or an
  // "invalid" result object if it failed because the callback has completed.
  TransactionGetResult Get(const DocumentReference& doc);

  // Calls `Update()`, `Set()` or `Delete()`, respectively, on the encapsulated
  // `Transaction` object with the given arguments. Returns `true` if the call
  // succeeded or `false` if it failed because the callback has completed.
  bool Update(const DocumentReference& doc, const FieldValue& field_value);
  bool Update(const DocumentReference& doc,
              const Map<std::string, FieldValue>& wrapper);
  bool Update(const DocumentReference& doc,
              const Map<FieldPath, FieldValue>& wrapper);
  bool Set(const DocumentReference& doc, const FieldValue& data,
           const SetOptions& options);
  bool Delete(const DocumentReference& doc);

  // Notifies the `TransactionManager` that the callback has completed.
  //
  // The `callback_successful` argument is `true` if the callback succeeded or
  // `false` otherwise.
  //
  // This method must be invoked at least once to wake up the blocked
  // transaction thread and complete the callback. Subsequent invocations,
  // although allowed, have no effect.
  void OnCompletion(bool callback_successful);

 private:
  std::shared_ptr<TransactionCallbackInternal> internal_;
  int32_t callback_id_ = -1;
  TransactionCallbackFn callback_ = nullptr;
};

// Bridges the C++ transaction API to the C# transaction API.
//
// This class is thread safe.
class TransactionManager {
 public:
  explicit TransactionManager(Firestore* firestore);

  ~TransactionManager();

  // Shuts down this object.
  //
  // This method will cause any in-flight transactions to immediately fail and
  // unblock any C++ transaction callback threads that are awaiting the
  // completion of the transactions.
  //
  // This method is idempotent and may be safely called multiple times.
  void Dispose();

  // Runs a transaction.
  //
  // The `callback` function will be invoked to perform the transaction, and the
  // `callback_id()` method of the `TransactionCallback` object specified to it
  // will contain the given `callback_id`.
  //
  // This method uses the `RunTransaction()` method of the `Firestore` instance
  // that was specified to the constructor to actually run the transaction.
  Future<void> RunTransaction(int32_t callback_id,
                              TransactionCallbackFn callback);

 private:
  static void CleanUp(void* object);

  std::shared_ptr<TransactionManagerInternal> internal_;
  CleanupNotifier& cleanup_notifier_;
  std::mutex dispose_mutex_;
};

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TRANSACTION_MANAGER_H_
