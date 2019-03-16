/*
 * Copyright 2017 Google LLC
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
 * Implements IdTokenListener by redirecting calls into C++.
 */
public class JniIdTokenListener implements FirebaseAuth.IdTokenListener {
  /**
   * Constructor is called via JNI from C++.
   * The `long` value is actually a pointer to the data we pass back to
   * the caller in onIdTokenChanged(). It provides the caller with context.
   */
  public JniIdTokenListener(long callbackData) {
    this.callbackData = callbackData;
  }

  @Override
  public void onIdTokenChanged(FirebaseAuth auth) {
    AuthCommon.safeRunNativeMethod(new Runnable() {
        @Override
        public void run() {
          nativeOnIdTokenChanged(callbackData);
        }
      });
  }

  /**
   * This function is implemented in the Auth C++ library (auth_android.cc).
   */
  private native void nativeOnIdTokenChanged(long callbackData);

  private long callbackData;
}
