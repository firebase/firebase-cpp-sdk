/*
 * Copyright 2016 Google LLC
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

package com.google.firebase.auth.internal.cpp;

import com.google.firebase.auth.FirebaseAuth;

/**
 * Implements AuthStateListener by redirecting calls into C++.
 */
public class JniAuthStateListener implements FirebaseAuth.AuthStateListener {

  /**
   * Lock that controls access to authData.
   */
  private final Object lock = new Object();

  /**
   * Pointer to an AuthData structure.
   */
  private long cppAuthData;

  /**
   * Constructor is called via JNI from C++.
   * cppAuthData is a pointer to the AuthData structure which is passed back to the caller in
   * onAuthStateChanged(). It provides the caller with context.
   */
  public JniAuthStateListener(long cppAuthData) {
    this.cppAuthData = cppAuthData;
  }

  /**
   * Remove the reference to the C++ AuthData object
   */
  public void disconnect() {
    synchronized (lock) {
      cppAuthData = 0;
    }
  }

  @Override
  public void onAuthStateChanged(FirebaseAuth auth) {
    AuthCommon.safeRunNativeMethod(new Runnable() {
        @Override
        public void run() {
          synchronized (lock) {
            if (cppAuthData != 0) {
              nativeOnAuthStateChanged(cppAuthData);
            }
          }
        }
      });
  }

  /**
   * This function is implemented in the Auth C++ library (auth_android.cc).
   */
  private native void nativeOnAuthStateChanged(long cppAuthData);
}
