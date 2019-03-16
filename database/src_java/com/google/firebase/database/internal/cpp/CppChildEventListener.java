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

import com.google.firebase.database.ChildEventListener;
import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;

/** Implements Firebase ChildEventListener by redirecting calls into C++. */
public class CppChildEventListener extends CppEventListener implements ChildEventListener {
  /** Construct a CppValueEventListener. Parameters are C++ pointers. */
  public CppChildEventListener(long cppDatabaseObject, long cppListenerObject) {
    super(cppDatabaseObject, cppListenerObject);
  }

  @Override
  public void onCancelled(DatabaseError error) {
    synchronized (lockObject) {
      if (cppDatabaseObject != 0 && cppListenerObject != 0) {
        nativeOnCancelled(cppDatabaseObject, cppListenerObject, error);
      }
    }
  }

  @Override
  public void onChildAdded(DataSnapshot snapshot, String previousChildName) {
    synchronized (lockObject) {
      if (cppDatabaseObject != 0 && cppListenerObject != 0) {
        nativeOnChildAdded(cppDatabaseObject, cppListenerObject, snapshot, previousChildName);
      }
    }
  }

  @Override
  public void onChildChanged(DataSnapshot snapshot, String previousChildName) {
    synchronized (lockObject) {
      if (cppDatabaseObject != 0 && cppListenerObject != 0) {
        nativeOnChildChanged(cppDatabaseObject, cppListenerObject, snapshot, previousChildName);
      }
    }
  }

  @Override
  public void onChildMoved(DataSnapshot snapshot, String previousChildName) {
    synchronized (lockObject) {
      if (cppDatabaseObject != 0 && cppListenerObject != 0) {
        nativeOnChildMoved(cppDatabaseObject, cppListenerObject, snapshot, previousChildName);
      }
    }
  }

  @Override
  public void onChildRemoved(DataSnapshot snapshot) {
    synchronized (lockObject) {
      if (cppDatabaseObject != 0 && cppListenerObject != 0) {
        nativeOnChildRemoved(cppDatabaseObject, cppListenerObject, snapshot);
      }
    }
  }

  /** This native function is implemented in the Firebase Database C++ library (util_android.cc). */
  private static native void nativeOnCancelled(
      long databaseObject, long listenerObject, DatabaseError error);

  /** This native function is implemented in the Firebase Database C++ library (util_android.cc). */
  private static native void nativeOnChildAdded(
      long databaseObject, long listenerObject, DataSnapshot snapshot, String previousChildName);

  /** This native function is implemented in the Firebase Database C++ library (util_android.cc). */
  private static native void nativeOnChildChanged(
      long databaseObject, long listenerObject, DataSnapshot snapshot, String previousChildName);

  /** This native function is implemented in the Firebase Database C++ library (util_android.cc). */
  private static native void nativeOnChildMoved(
      long databaseObject, long listenerObject, DataSnapshot snapshot, String previousChildName);

  /** This native function is implemented in the Firebase Database C++ library (util_android.cc). */
  private static native void nativeOnChildRemoved(
      long databaseObject, long listenerObject, DataSnapshot snapshot);
}
