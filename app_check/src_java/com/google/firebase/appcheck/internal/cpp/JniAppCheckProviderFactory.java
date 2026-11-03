/*
 * Copyright 2023 Google LLC
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

package com.google.firebase.appcheck.internal.cpp;

import com.google.firebase.FirebaseApp;
import com.google.firebase.appcheck.AppCheckProvider;
import com.google.firebase.appcheck.AppCheckProviderFactory;

/**
 * Java AppCheckProviderFactory that uses a C++ AppCheckProviderFactory internally.
 */
public class JniAppCheckProviderFactory implements AppCheckProviderFactory {
  // Pointer to a C++ AppCheckProviderFactory.
  private volatile long cFactory;
  // Pointer to a C++ App.
  private volatile long cApp;

  public JniAppCheckProviderFactory(long cFactory, long cApp) {
    this.cFactory = cFactory;
    this.cApp = cApp;
  }

  /**
   * Disconnects this Java wrapper from the underlying C++ factory and app pointers so subsequent
   * {@link #create(FirebaseApp)} calls cannot dereference destroyed native objects.
   */
  public synchronized void disconnect() {
    this.cFactory = 0L;
    this.cApp = 0L;
  }

  /**
   * Creates an {@link AppCheckProvider} backed by the native C++ factory.
   *
   * <p>Always returns a non-null {@link JniAppCheckProvider} to satisfy the Android
   * {@code AppCheckProviderFactory.create()} contract. If this factory has been disconnected,
   * {@code firebaseApp} is null, or native provider creation fails, a {@code JniAppCheckProvider(0L)}
   * instance is returned whose token requests fail their {@code Task} cleanly in JNI rather than
   * throwing a {@code NullPointerException} in {@code DefaultFirebaseAppCheck}.
   */
  @Override
  public synchronized AppCheckProvider create(FirebaseApp firebaseApp) {
    if (firebaseApp == null || cFactory == 0L || cApp == 0L) {
      return new JniAppCheckProvider(0L);
    }
    // Call the C++ Factory's CreateProvider to get a C++ provider pointer.
    long cProvider = nativeCreateProvider(cFactory, cApp);
    // Create a Java provider to wrap the created C++ provider (or 0L on failure, which fails
    // token tasks cleanly in nativeGetToken/nativeGetLimitedUseToken).
    return new JniAppCheckProvider(cProvider);
  }

  /**
   * This function is implemented in the AppCheck C++ library (app_check_android.cc).
   */
  private native long nativeCreateProvider(long cFactory, long cApp);
}
