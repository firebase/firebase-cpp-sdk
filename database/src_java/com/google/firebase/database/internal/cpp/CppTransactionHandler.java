// Copyright 2016 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.firebase.database.internal.cpp;

import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;
import com.google.firebase.database.MutableData;
import com.google.firebase.database.Transaction;

/** Implements Firebase Transaction.Handler base class, by managing pointers into C++. */
public class CppTransactionHandler implements Transaction.Handler {
  protected long cppDatabaseObject = 0;
  protected long cppTransactionObject = 0;
  protected final Object lockObject = new Object();

  /** Construct a CppTransactionHandler. Parameters are C++ pointers. */
  public CppTransactionHandler(long cppDatabaseObject, long cppTransactionObject) {
    this.cppDatabaseObject = cppDatabaseObject;
    this.cppTransactionObject = cppTransactionObject;
  }

  /**
   * Discard native pointers from this instance. Returns cppTransactionObject, which the C++ code
   * will delete.
   */
  public long discardPointers() {
    synchronized (lockObject) {
      long transactionObject = this.cppTransactionObject;
      this.cppDatabaseObject = 0;
      this.cppTransactionObject = 0;
      return transactionObject;
    }
  }

  @Override
  public Transaction.Result doTransaction(MutableData currentData) {
    synchronized (lockObject) {
      if (cppDatabaseObject != 0 && cppTransactionObject != 0) {
        MutableData newData =
            nativeDoTransaction(cppDatabaseObject, cppTransactionObject, currentData);
        if (newData != null) {
          return Transaction.success(newData);
        } else {
          return Transaction.abort();
        }
      }
    }
    return Transaction.abort();
  }

  @Override
  public void onComplete(DatabaseError error, boolean committed, DataSnapshot currentData) {
    synchronized (lockObject) {
      if (cppDatabaseObject != 0 && cppTransactionObject != 0) {
        nativeOnComplete(cppDatabaseObject, cppTransactionObject, error, committed, currentData);
      }
    }
  }

  /** This native function is implemented in the Firebase Database C++ library (util_android.cc). */
  private static native MutableData nativeDoTransaction(
      long databaseObject, long transactionObject, MutableData currentData);

  /** This native function is implemented in the Firebase Database C++ library (util_android.cc). */
  private static native void nativeOnComplete(
      long databaseObject,
      long transactionObject,
      DatabaseError error,
      boolean committed,
      DataSnapshot currentData);
}
