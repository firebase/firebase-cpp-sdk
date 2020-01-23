package com.google.firebase.firestore.internal.cpp;

import android.support.annotation.Nullable;
import com.google.firebase.firestore.EventListener;
import com.google.firebase.firestore.FirebaseFirestoreException;
import com.google.firebase.firestore.QuerySnapshot;

/** Implements Firestore EventListener<QuerySnapshot> by redirecting calls into C++. */
public class QueryEventListener extends CppEventListener implements EventListener<QuerySnapshot> {
  /** Construct a QueryEventListener. Parameters are C++ pointers. */
  public QueryEventListener(long cppFirestoreObject, long cppListenerObject) {
    super(cppFirestoreObject, cppListenerObject);
  }

  @Override
  public synchronized void onEvent(
      @Nullable QuerySnapshot value, @Nullable FirebaseFirestoreException error) {
    if (cppFirestoreObject != 0 && cppListenerObject != 0) {
      nativeOnEvent(cppFirestoreObject, cppListenerObject, value, error);
    }
  }

  /**
   * This native function is implemented in the Firestore C++ library (event_listener_android.cc).
   */
  private static native void nativeOnEvent(
      long firestoreObject,
      long listenerObject,
      @Nullable Object value,
      @Nullable FirebaseFirestoreException error);
}
