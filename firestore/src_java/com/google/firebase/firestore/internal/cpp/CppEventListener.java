package com.google.firebase.firestore.internal.cpp;

/** Implements Firestore EventListener base class. */
public class CppEventListener {
  protected long cppFirestoreObject = 0;
  protected long cppListenerObject = 0;

  /** Construct a CppEventListener. Parameters are C++ pointers. */
  public CppEventListener(long cppFirestoreObject, long cppListenerObject) {
    this.cppFirestoreObject = cppFirestoreObject;
    this.cppListenerObject = cppListenerObject;
  }

  /** Discard native pointers from this instance. */
  public synchronized void discardPointers() {
    this.cppFirestoreObject = 0;
    this.cppListenerObject = 0;
  }
}
