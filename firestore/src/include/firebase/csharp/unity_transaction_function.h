#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_UNITY_TRANSACTION_FUNCTION_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_UNITY_TRANSACTION_FUNCTION_H_

#include <cstdint>
#include <string>

#include "app/src/mutex.h"
#include "firebase/firestore.h"
#include "firebase/firestore/transaction.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {
namespace csharp {

// Add this to make this header compile when SWIG is not involved.
#ifndef SWIGSTDCALL
#if !defined(SWIG) && \
    (defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__))
#define SWIGSTDCALL __stdcall
#else
#define SWIGSTDCALL
#endif
#endif

/**
 * Type of the C# delegate that we will forward transaction Apply() calls to.
 */
typedef Error(SWIGSTDCALL* UnityTransactionFunctionCallback)(
    int callback_id, void* transaction, std::string* error_message);

/**
 * A (C++) implementation of TransactionFunction that forwards the calls back
 * to a C# delegate.
 */
class UnityTransactionFunction : public TransactionFunction {
 public:
  /**
   * Called by C# to register the global delegate that should receive
   * transaction callbacks.
   */
  static void SetCallback(UnityTransactionFunctionCallback callback);

  /**
   * Called by C# to start a transaction on the provided Firestore instance,
   * using the specified callback_id to identify it.
   */
  static Future<void> RunTransactionOn(int32_t callback_id,
                                       Firestore* firestore);

  /**
   * Implementation of TransactionFunction::Apply() that forwards to the C#
   * global delegate, passing along this->callback_id_ for context.
   */
  Error Apply(Transaction* transaction, std::string* error_message) override;

 private:
  explicit UnityTransactionFunction(int32_t callback_id)
      : callback_id_(callback_id) {}

  int32_t callback_id_ = -1;

  static Mutex* mutex_;
  static UnityTransactionFunctionCallback transaction_function_callback_;
};

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_UNITY_TRANSACTION_FUNCTION_H_
