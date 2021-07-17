// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_COMMON_TRANSACTION_FUNCTION_H_
#define FIREBASE_FIRESTORE_SRC_COMMON_TRANSACTION_FUNCTION_H_

#include <string>

#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {

class Transaction;

/**
 * An interface for providing code to be executed within a transaction
 * context.
 */
class TransactionFunction {
 public:
  virtual ~TransactionFunction() {}

  /**
   * Subclass should override this method and put the transaction logic here.
   *
   * @param[in] transaction The transaction to run this function with.
   * @param[out] error_message You can set error message with this parameter.
   * @return Either Error::kErrorOk if successful or the error code from Error
   * that most closely matches the failure.
   */
  virtual Error Apply(Transaction& transaction, std::string& error_message) = 0;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_TRANSACTION_FUNCTION_H_
