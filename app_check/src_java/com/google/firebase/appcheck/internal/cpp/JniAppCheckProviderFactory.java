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
  // Pointer to a C++ AppCheckProviderFactory
  private long cFactory;
  // Pointer to a C++ App
  private long cApp;

  public JniAppCheckProviderFactory(long cFactory, long cApp) {
    this.cFactory = cFactory;
    this.cApp = cApp;
  }

  @Override
  public AppCheckProvider create(FirebaseApp firebaseApp) {
    // Call the C++ Factory's CreateProvider to get a C++ provider pointer.
    long cProvider = nativeCreateProvider(cFactory, cApp);

    // Create a java provider to wrap the created C++ provider, and return that.
    return new JniAppCheckProvider(cProvider);
  }

  /**
   * This function is implemented in the AppCheck C++ library (app_check_android.cc).
   */
  private native long nativeCreateProvider(long cFactory, long cApp);
}
