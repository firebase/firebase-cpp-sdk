package com.google.firebase.firestore.internal.cpp;

/**
 * An implementation of {@code Runnable} that delegates to a {@code
 * firebase::firestore::EventListener<void>}. This is useful for getting callbacks from Java APIs
 * that cannot fail, like addSnapshotsInSyncListener.
 */
public class VoidEventListener extends CppEventListener implements Runnable {
  /**
   * Constructs a VoidEventListener. Ownership of the underlying EventListener can be transferred
   * when creating the {@code ListenerRegistration}. If the ListenerRegistration owns the
   * EventListener, it will de-allocate the EventListener in its destructor.
   *
   * <p>Passing in 0 is considered a null pointer and will result in {@code run} becoming a no-op.
   *
   * @param cppListenerObject Pointer to a {@code firebase::firestore::EventListener<void>}.
   */
  public VoidEventListener(long cppListenerObject) {
    super(0, cppListenerObject);
  }

  @Override
  public void run() {
    nativeOnEvent(cppListenerObject);
  }

  /**
   * Interprets the {@code listenerObject} as a {@code firebase::firestore::EventListener<void>} and
   * invokes the listener's {@code OnEvent} method with {@code Error::kOk}. The EventListener will
   * never be passed anything other than Ok because VoidEventListener can only be used in
   * circumstances where the callback can't fail.
   *
   * <p>This native method is implemented in the Firestore C++ library {@code
   * event_listener_android.cc}.
   */
  private static native void nativeOnEvent(long listenerObject);
}
