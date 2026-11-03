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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import com.google.firebase.firestore.FirebaseFirestoreException;
import com.google.firebase.firestore.FirebaseFirestoreException.Code;
import com.google.firebase.firestore.Transaction;

/**
 * Implements Firestore {@code Transaction.Function} by redirecting calls into the C++ equivalent.
 */
public class TransactionFunction
    extends CppEventListener implements Transaction.Function<Object> {
  public TransactionFunction(long cppFirestoreObject, long cppTransactionFunctionObject) {
    super(cppFirestoreObject, cppTransactionFunctionObject);
  }

  @Nullable
  @Override
  public Object apply(@NonNull Transaction transaction)
      throws FirebaseFirestoreException {
    long firestorePtr;
    long listenerPtr;
    synchronized (this) {
      firestorePtr = cppFirestoreObject;
      listenerPtr = cppListenerObject;
    }
    if (firestorePtr != 0 && listenerPtr != 0) {
      try {
        byte[] result = nativeApply(firestorePtr, listenerPtr, transaction);
        if (result == null) {
          FirebaseFirestoreException exception =
              (FirebaseFirestoreException) nativeGetException(firestorePtr, listenerPtr);
          throw exception;
        }
      } catch (FirebaseFirestoreException e) {
        throw e;
      } catch (RuntimeException e) {
        throw e;
      } catch (Exception e) {
        throw new FirebaseFirestoreException(e.getMessage(), Code.UNKNOWN, e);
      }
    }
    return null;
  }

  /**
   * Interprets the {@code transactionFunctionObject} as a {@code
   * firebase::firestore::TransactionFunction} and invokes its {@code apply} method with the {@code
   * transaction} created by interpreting the {@code firestoreObject} as a {@code
   * firebase::firestore::Firestore}.
   *
   * @return A boolean value indicating whether the C++ transaction function succeeded.
   */
  private static native byte[] nativeApply(
      long firestoreObject, long transactionFunctionObject, Object transaction);

  private static native Object nativeGetException(
      long firestoreObject, long transactionFunctionObject);
}
