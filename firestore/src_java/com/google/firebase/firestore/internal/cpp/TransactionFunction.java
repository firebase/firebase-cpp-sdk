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

package com.google.firebase.firestore.internal.cpp;

import androidx.annotation.Nullable;
import com.google.firebase.firestore.FirebaseFirestoreException;
import com.google.firebase.firestore.FirebaseFirestoreException.Code;
import com.google.firebase.firestore.Transaction;

/**
 * Implements Firestore TransactionFunction by redirecting calls into C++. This
 * shares the same logic as a listener but there is difference. A listener
 * implements onEvent() while Transaction Function implements apply().
 */
public class TransactionFunction
    extends CppEventListener implements Transaction.Function<Object> {
  /** Construct a TransactionFunction. Parameters are C++ pointers. */
  public TransactionFunction(
      long cppFirestoreObject, long cppTransactionFunctionObject) {
    super(cppFirestoreObject, cppTransactionFunctionObject);
  }

  @Override
  public Object apply(Transaction transaction)
      throws FirebaseFirestoreException {
    if (cppFirestoreObject != 0 && cppListenerObject != 0) {
      try {
        Exception exception =
            nativeApply(cppFirestoreObject, cppListenerObject, transaction);
        if (exception != null) {
          throw exception;
        }
      } catch (FirebaseFirestoreException e) {
        e.printStackTrace();
        throw e;
      } catch (RuntimeException e) {
        e.printStackTrace();
        throw e;
      } catch (Exception e) {
        e.printStackTrace();
        throw new FirebaseFirestoreException(e.getMessage(), Code.UNKNOWN, e);
      }
    }
    return null;
  }

  /**
   * This native function is implemented in the Firestore C++ library
   * (transaction_android.cc).
   */
  @Nullable
  private static native Exception nativeApply(long firestoreObject,
      long transactionFunctionObject,
      Transaction transaction);
}
