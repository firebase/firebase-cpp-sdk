/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_LAMBDA_TRANSACTION_FUNCTION_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_LAMBDA_TRANSACTION_FUNCTION_H_

#include <functional>
#include <string>

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/common/transaction_function.h"
#include "firestore/src/include/firebase/firestore/transaction.h"

namespace firebase {
namespace firestore {

/**
 * A particular TransactionFunction type that wraps an update function provided
 * by user in the form of lambda. In other word, this class is used to implement
 * Firestore::RunTransaction() where lambda is passed as a parameter.
 *
 * This class may also be able to wrap other types, see @varconst comment in
 * CL/220371659.
 */
class LambdaTransactionFunction
    : public TransactionFunction,
      public Promise<void, void, FirestoreInternal::AsyncFn>::Completion {
 public:
  LambdaTransactionFunction(
      std::function<Error(Transaction&, std::string&)> update)
      : update_(firebase::Move(update)) {
    FIREBASE_ASSERT(update_);
  }

  // Override TransactionFunction::Apply().
  Error Apply(Transaction& transaction, std::string& error_message) override {
    Error result = update_(transaction, error_message);
    return result;
  }

  // Override Promise::Completion::CompleteWith().
  void CompleteWith(Error error_code,
                    const char* error_message,
                    void* result) override {
    delete this;
  }

 private:
  std::function<Error(Transaction&, std::string&)> update_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_LAMBDA_TRANSACTION_FUNCTION_H_
