package com.google.firebase.firestore.internal.cpp;

import androidx.annotation.Nullable;
import com.google.firebase.firestore.DocumentSnapshot;
import com.google.firebase.firestore.EventListener;
import com.google.firebase.firestore.FirebaseFirestoreException;

/**
 * Implements Firestore {@code
 * firebase::firestore::EventListener<firebase::firestore::DocumentSnapshot>} by redirecting calls
 * into the C++ equivalent.
 */
public class DocumentEventListener extends CppEventListener
    implements EventListener<DocumentSnapshot> {
  /**
   * Constructs a DocumentEventListener. Ownership of the underlying EventListener can be
   * transferred when creating the {@code ListenerRegistration}. If the ListenerRegistration owns
   * the EventListener, it will de-allocate the EventListener in its destructor.
   *
   * <p>Passing in 0 is considered a null pointer and will result in {@code onEvent} becoming a
   * no-op.
   *
   * @param cppFirestoreObject Pointer to a {@code firebase::firestore::Firestore}.
   * @param cppListenerObject Pointer to a {@code
   *     firebase::firestore::EventListener<firebase::firestore::DocumentSnapshot>}.
   */
  public DocumentEventListener(long cppFirestoreObject, long cppListenerObject) {
    super(cppFirestoreObject, cppListenerObject);
  }

  @Override
  public synchronized void onEvent(
      @Nullable DocumentSnapshot value, @Nullable FirebaseFirestoreException error) {
      nativeOnEvent(cppFirestoreObject, cppListenerObject, value, error);
  }

  /**
   * Interprets the {@code listenerObject} as a {@code
   * firebase::firestore::EventListener<DocumentSnapshot>} and invokes the listener's {@code
   * OnEvent} method with the {@code error} and {@code DocumentSnapshot} created by interpreting the
   * {@code firestoreObject} as a {@code firebase::firestore::Firestore}.
   *
   * <p>This native method is implemented in the Firestore C++ library {@code
   * event_listener_android.cc}.
   */
  private static native void nativeOnEvent(
      long firestoreObject,
      long listenerObject,
      @Nullable Object value,
      @Nullable FirebaseFirestoreException error);
}
