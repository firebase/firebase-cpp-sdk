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

public class JniAppCheckProviderFactory implements AppCheckProviderFactory {
  private long cFactory;

  public JniAppCheckProviderFactory(long cFactory) {
    this.cFactory = cFactory;
  }

  @Override
  public AppCheckProvider create(FirebaseApp firebaseApp) {
    // Call C++ factory.createprovider to get a C++ provider
    // Note: other examples of calling c++ from java does not appear
    // to have a return value. Maybe I want an output parameter.
    // one example jint

    // for messaging, we define a single java listener and cpp logic calls into
    // relevant cpp logic to notify cpp listeners
    // could in cpp use app to determine which provider is relevant
    long cProvider = nativeCreateProvider(cFactory, firebaseApp);

    // Create a java provider to wrap the created C++ provider, and return that.
    return new JniAppCheckProvider(cProvider);
  }

  /**
   * This function is implemented in the AppCheck C++ library (app_check_android.cc).
   */
  private native long nativeCreateProvider(long cFactory, FirebaseApp firebaseApp);
}