#include "firebase/csharp/unity_transaction_function.h"

#include "app/src/assert.h"

namespace firebase {
namespace firestore {
namespace csharp {

::firebase::Mutex* UnityTransactionFunction::mutex_ = new ::firebase::Mutex();
UnityTransactionFunctionCallback
    UnityTransactionFunction::transaction_function_callback_ = nullptr;

Error UnityTransactionFunction::Apply(Transaction* transaction,
                                      std::string* error_message) {
  MutexLock lock(*mutex_);
  if (transaction_function_callback_ != nullptr) {
    return transaction_function_callback_(callback_id_, transaction,
                                          error_message);
  } else {
    FIREBASE_ASSERT_MESSAGE(
        false, "C++ transaction callback called before C# registered.");
    return Error::kOk;
  }
}

void UnityTransactionFunction::SetCallback(
    UnityTransactionFunctionCallback callback) {
  MutexLock lock(*mutex_);
  if (!callback) {
    transaction_function_callback_ = nullptr;
    return;
  }

  if (transaction_function_callback_) {
    FIREBASE_ASSERT(transaction_function_callback_ == callback);
  } else {
    transaction_function_callback_ = callback;
  }
}

Future<void> UnityTransactionFunction::RunTransactionOn(int32_t callback_id,
                                                        Firestore* firestore) {
  UnityTransactionFunction unity_transaction_function(callback_id);
  return firestore->RunTransaction(
      [unity_transaction_function](Transaction* transaction,
                                   std::string* error_message) mutable {
        return unity_transaction_function.Apply(transaction, error_message);
      });
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
