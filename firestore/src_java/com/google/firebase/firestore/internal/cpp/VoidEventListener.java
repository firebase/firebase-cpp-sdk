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

/**
 * An implementation of {@code Runnable} that delegates to a {@code
 * firebase::firestore::EventListener<void>}. This is useful for getting
 * callbacks from Java APIs that cannot fail, like addSnapshotsInSyncListener.
 */
public class VoidEventListener extends CppEventListener implements Runnable {
  /**
   * Constructs a VoidEventListener. Ownership of the underlying EventListener
   * can be transferred when creating the {@code ListenerRegistration}. If the
   * ListenerRegistration owns the EventListener, it will de-allocate the
   * EventListener in its destructor.
   *
   * <p>Passing in 0 is considered a null pointer and will result in {@code run}
   * becoming a no-op.
   *
   * @param cppListenerObject Pointer to a {@code
   *     firebase::firestore::EventListener<void>}.
   */
  public VoidEventListener(long cppListenerObject) {
    super(0, cppListenerObject);
  }

  @Override
  public void run() {
    nativeOnEvent(cppListenerObject);
  }

  /**
   * Interprets the {@code listenerObject} as a {@code
   * firebase::firestore::EventListener<void>} and invokes the listener's {@code
   * OnEvent} method with {@code Error::kOk}. The EventListener will never be
   * passed anything other than Ok because VoidEventListener can only be used in
   * circumstances where the callback can't fail.
   *
   * <p>This native method is implemented in the Firestore C++ library {@code
   * event_listener_android.cc}.
   */
  private static native void nativeOnEvent(long listenerObject);
}
