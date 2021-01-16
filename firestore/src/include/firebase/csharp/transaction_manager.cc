#include "firebase/csharp/transaction_manager.h"

#include <condition_variable>  // NOLINT(build/c++11)
#include <unordered_set>

#include "app/src/assert.h"
#include "app/src/callback.h"
#include "firebase/firestore/document_reference.h"
#include "firebase/firestore/field_path.h"
#include "firebase/firestore/field_value.h"
#include "firebase/firestore/set_options.h"
#include "firebase/firestore/transaction.h"

#if defined(__ANDROID__)
#include "firestore/src/android/firestore_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/firestore_stub.h"
#else
#include "firestore/src/ios/firestore_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {
namespace csharp {

class TransactionCallbackInternal {
 public:
  explicit TransactionCallbackInternal(Transaction& transaction)
      : transaction_(transaction) {}

  void OnCompletion(bool callback_successful) {
    std::lock_guard<std::mutex> lock(completion_mutex_);
    if (is_completed_) {
      return;
    }
    is_completed_ = true;
    result_ = callback_successful;
    completion_condition_.notify_all();
  }

  // Waits for `OnCompletion()` to be invoked.
  //
  // If `OnCompletion()` has already been invoked then this method returns
  // immediately; otherwise, it blocks waiting for it to be invoked.
  //
  // Returns the value that was specified to `OnCompletion()` for its argument.
  bool AwaitCompletion() {
    std::unique_lock<std::mutex> lock(completion_mutex_);
    completion_condition_.wait(lock, [&] { return is_completed_; });
    return result_;
  }

  // Marks the `Transaction` reference that was given to the constructor as
  // "invalid".
  //
  // After this method is invoked, all transaction-related methods in this class
  // (i.e. `Get()`, `Set()`, `Update()`, and `Delete()`) will simply return a
  // "failed" value and will not actually invoke methods on the encapsulated
  // `Transaction` reference.
  //
  // This method will block until all in-flight transaction-related methods
  // at the time of invocation have completed.
  void InvalidateTransaction() {
    std::lock_guard<std::mutex> transaction_lock(transaction_mutex_);
    is_transaction_valid_ = false;
  }

  TransactionGetResult Get(const DocumentReference& doc) {
    std::lock_guard<std::mutex> transaction_lock(transaction_mutex_);
    if (!is_transaction_valid_) {
      return {};
    }
    Error error_code = Error::kErrorUnknown;
    std::string error_message;
    DocumentSnapshot snapshot =
        transaction_.Get(doc, &error_code, &error_message);
    return {std::move(snapshot), error_code, std::move(error_message)};
  }

  bool Update(const DocumentReference& doc, const FieldValue& field_value) {
    std::lock_guard<std::mutex> transaction_lock(transaction_mutex_);
    if (!is_transaction_valid_) {
      return false;
    }
    transaction_.Update(doc, field_value.map_value());
    return true;
  }

  bool Update(const DocumentReference& doc,
              const Map<std::string, FieldValue>& wrapper) {
    std::lock_guard<std::mutex> transaction_lock(transaction_mutex_);
    if (!is_transaction_valid_) {
      return false;
    }
    transaction_.Update(doc, wrapper.Unwrap());
    return true;
  }

  bool Update(const DocumentReference& doc,
              const Map<FieldPath, FieldValue>& wrapper) {
    std::lock_guard<std::mutex> transaction_lock(transaction_mutex_);
    if (!is_transaction_valid_) {
      return false;
    }
    transaction_.Update(doc, wrapper.Unwrap());
    return true;
  }

  bool Set(const DocumentReference& doc, const FieldValue& data,
           const SetOptions& options) {
    std::lock_guard<std::mutex> transaction_lock(transaction_mutex_);
    if (!is_transaction_valid_) {
      return false;
    }
    transaction_.Set(doc, data.map_value(), options);
    return true;
  }

  bool Delete(const DocumentReference& doc) {
    std::lock_guard<std::mutex> transaction_lock(transaction_mutex_);
    if (!is_transaction_valid_) {
      return false;
    }
    transaction_.Delete(doc);
    return true;
  }

 private:
  // TODO(b/172565676) Change `transaction_mutex_` from an exclusive mutex to a
  // shared mutex to enable concurrent method invocations on `transaction_`.
  std::mutex transaction_mutex_;
  Transaction& transaction_;
  bool is_transaction_valid_ = true;

  std::mutex completion_mutex_;
  std::condition_variable completion_condition_;
  bool is_completed_ = false;
  bool result_ = false;
};

class TransactionManagerInternal
    : public std::enable_shared_from_this<TransactionManagerInternal> {
 public:
  explicit TransactionManagerInternal(Firestore* firestore)
      : firestore_(firestore) {
    FIREBASE_ASSERT(firestore);
  }

  ~TransactionManagerInternal() {
    std::lock_guard<std::mutex> lock(mutex_);
    FIREBASE_ASSERT(is_disposed_);
    FIREBASE_ASSERT(running_callbacks_.empty());
  }

  void Dispose() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_disposed_ = true;
    for (TransactionCallbackInternal* running_callback : running_callbacks_) {
      running_callback->OnCompletion(false);
    }
  }

  Future<void> RunTransaction(int32_t callback_id,
                              TransactionCallbackFn callback_fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_disposed_) {
      return {};
    }

    auto shared_this = shared_from_this();
    return firestore_->RunTransaction([shared_this, callback_id, callback_fn](
                                          Transaction& transaction,
                                          std::string& error_message) {
      if (shared_this->ExecuteCallback(callback_id, callback_fn, transaction)) {
        return Error::kErrorOk;
      } else {
        // Return a non-retryable error code.
        return Error::kErrorInvalidArgument;
      }
    });
  }

 private:
  bool ExecuteCallback(int32_t callback_id, TransactionCallbackFn callback_fn,
                       Transaction& transaction) {
    auto transaction_callback_internal =
        std::make_shared<TransactionCallbackInternal>(transaction);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (is_disposed_) {
        return false;
      }
      running_callbacks_.insert(transaction_callback_internal.get());
    }

    auto transaction_callback =
        // NOLINTNEXTLINE(modernize-make-unique)
        std::unique_ptr<TransactionCallback>(new TransactionCallback(
            transaction_callback_internal, callback_id, callback_fn));
    callback::AddCallback(
        new callback::CallbackMoveValue1<std::unique_ptr<TransactionCallback>>(
            std::move(transaction_callback), ExecuteCallbackFromMainThread));
    bool result = transaction_callback_internal->AwaitCompletion();
    transaction_callback_internal->InvalidateTransaction();

    {
      std::lock_guard<std::mutex> lock(mutex_);
      running_callbacks_.erase(transaction_callback_internal.get());
    }

    return result;
  }

  static void ExecuteCallbackFromMainThread(
      std::unique_ptr<TransactionCallback>* transaction_callback) {
    TransactionCallbackFn callback_fn = (*transaction_callback)->callback();
    std::shared_ptr<TransactionCallbackInternal> transaction_callback_internal =
        (*transaction_callback)->internal();
    bool successful = callback_fn(transaction_callback->release());
    if (!successful) {
      transaction_callback_internal->OnCompletion(false);
    }
  }

  std::mutex mutex_;
  Firestore* firestore_ = nullptr;
  bool is_disposed_ = false;
  // NOLINTNEXTLINE(google3-runtime-unneeded-pointer-stability-check)
  std::unordered_set<TransactionCallbackInternal*> running_callbacks_;
};

TransactionManager::TransactionManager(Firestore* firestore)
    : internal_(std::make_shared<TransactionManagerInternal>(firestore)),
      cleanup_notifier_(firestore->internal_->cleanup()) {
  cleanup_notifier_.RegisterObject(this, CleanUp);
}

TransactionManager::~TransactionManager() { Dispose(); }

void TransactionManager::CleanUp(void* object) {
  static_cast<TransactionManager*>(object)->Dispose();
}

void TransactionManager::Dispose() {
  std::lock_guard<std::mutex> lock(dispose_mutex_);
  if (!internal_) {
    return;
  }

  internal_->Dispose();
  internal_.reset();

  cleanup_notifier_.UnregisterObject(this);
}

Future<void> TransactionManager::RunTransaction(
    int32_t callback_id, TransactionCallbackFn callback_fn) {
  // Make a local copy of `internal_` since it could be reset asynchronously
  // by a call to `Dispose()`.
  std::shared_ptr<TransactionManagerInternal> internal_local = internal_;
  if (!internal_local) {
    return {};
  }
  return internal_local->RunTransaction(callback_id, callback_fn);
}

void TransactionCallback::OnCompletion(bool callback_successful) {
  internal_->OnCompletion(callback_successful);
}

bool TransactionCallback::Update(const DocumentReference& doc,
                                 const FieldValue& field_value) {
  return internal_->Update(doc, field_value);
}

bool TransactionCallback::Update(const DocumentReference& doc,
                                 const Map<std::string, FieldValue>& wrapper) {
  return internal_->Update(doc, wrapper);
}

bool TransactionCallback::Update(const DocumentReference& doc,
                                 const Map<FieldPath, FieldValue>& wrapper) {
  return internal_->Update(doc, wrapper);
}

bool TransactionCallback::Set(const DocumentReference& doc,
                              const FieldValue& data,
                              const SetOptions& options) {
  return internal_->Set(doc, data, options);
}

bool TransactionCallback::Delete(const DocumentReference& doc) {
  return internal_->Delete(doc);
}

TransactionGetResult TransactionCallback::Get(const DocumentReference& doc) {
  return internal_->Get(doc);
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
