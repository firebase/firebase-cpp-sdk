package com.google.firebase.firestore.internal.cpp;

import androidx.annotation.Nullable;
import com.google.firebase.firestore.FirebaseFirestoreException;
import com.google.firebase.firestore.FirebaseFirestoreException.Code;
import com.google.firebase.firestore.Transaction;

/**
 * Implements Firestore TransactionFunction by redirecting calls into C++. This shares the same
 * logic as a listener but there is difference. A listener implements onEvent() while Transaction
 * Function implements apply().
 */
public class TransactionFunction extends CppEventListener implements Transaction.Function<Object> {
  /** Construct a TransactionFunction. Parameters are C++ pointers. */
  public TransactionFunction(long cppFirestoreObject, long cppTransactionFunctionObject) {
    super(cppFirestoreObject, cppTransactionFunctionObject);
  }

  @Override
  public Object apply(Transaction transaction) throws FirebaseFirestoreException {
    if (cppFirestoreObject != 0 && cppListenerObject != 0) {
      try {
        Exception exception = nativeApply(cppFirestoreObject, cppListenerObject, transaction);
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

  /** This native function is implemented in the Firestore C++ library (transaction_android.cc). */
  @Nullable
  private static native Exception nativeApply(
      long firestoreObject, long transactionFunctionObject, Transaction transaction);
}
