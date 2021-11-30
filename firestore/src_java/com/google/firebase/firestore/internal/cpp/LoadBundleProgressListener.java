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
import com.google.firebase.firestore.EventListener;
import com.google.firebase.firestore.FirebaseFirestoreException;
import com.google.firebase.firestore.LoadBundleTaskProgress;
import com.google.firebase.firestore.OnProgressListener;

/**
 * Implements Firestore {@code
 * firebase::firestore::EventListener<firebase::firestore::LoadBundleTaskProgress>}
 * by redirecting calls into the C++ equivalent.
 */
public class LoadBundleProgressListener extends CppEventListener
    implements OnProgressListener<LoadBundleTaskProgress> {
  /**
   * Constructs a ProgressEventListener.
   *
   * <p>Passing in 0 is considered a null pointer and will result in {@code
   * onEvent} becoming a no-op.
   *
   * @param cppFirestoreObject Pointer to a {@code
   *     firebase::firestore::Firestore}.
   * @param cppListenerObject Pointer to a {@code
   *     firebase::firestore::EventListener<firebase::firestore::LoadBundleTaskProgress>}.
   */
  public LoadBundleProgressListener(
      long cppFirestoreObject, long cppListenerObject) {
    super(cppFirestoreObject, cppListenerObject);
  }

  @Override
  public synchronized void onProgress(
      @Nullable LoadBundleTaskProgress progress) {
    nativeOnProgress(cppFirestoreObject, cppListenerObject, progress);
  }

  /**
   * Interprets the {@code listenerObject} as a {@code
   * firebase::firestore::EventListener<LoadBundleTaskProgress>} and invokes the
   * listener's {@code OnEvent} method with {@code value} interpreted as (@code
   * LoadBundleTaskProgress).
   *
   * <p>This native method is implemented in the Firestore C++ library {@code
   * event_listener_android.cc}.
   */
  private static native void nativeOnProgress(
      long firestoreObject, long listenerObject, @Nullable Object value);
}
