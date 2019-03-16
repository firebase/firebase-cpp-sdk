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

import com.google.firebase.FirebaseException;
import com.google.firebase.auth.PhoneAuthCredential;
import com.google.firebase.auth.PhoneAuthProvider.ForceResendingToken;
import com.google.firebase.auth.PhoneAuthProvider.OnVerificationStateChangedCallbacks;

/**
 * Redirect PhoneAuthProvider callbacks into C++.
 */
public class JniAuthPhoneListener extends OnVerificationStateChangedCallbacks {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Synchronization object. Every function is synchronized so the class is effectively a monitor.
  private final Object lock;

  // C++ pointer to the user-owned PhoneAuthProvider::Listener.
  // When this object is destroyed, PhoneAuthProvider::Listener::~Listener() will call into
  // this class and reset this variable to nullptr.
  private long cListener;

  public JniAuthPhoneListener(long cListener) {
    lock = new Object();
    this.cListener = cListener;
  }

  public void disconnect() {
    synchronized (lock) {
      cListener = CPP_NULLPTR;
    }
  }

  @Override
  public void onVerificationCompleted(final PhoneAuthCredential credential) {
    synchronized (lock) {
      if (cListener != CPP_NULLPTR) {
        AuthCommon.safeRunNativeMethod(new Runnable() {
            @Override
            public void run() {
              nativeOnVerificationCompleted(cListener, credential);
            }
          });
      }
    }
  }

  @Override
  public void onVerificationFailed(final FirebaseException exception) {
    synchronized (lock) {
      if (cListener != CPP_NULLPTR) {
        AuthCommon.safeRunNativeMethod(new Runnable() {
            @Override
            public void run() {
              nativeOnVerificationFailed(cListener, exception.getMessage());
            }
          });
      }
    }
  }

  @Override
  public void onCodeSent(final String verificationId, final ForceResendingToken token) {
    synchronized (lock) {
      if (cListener != CPP_NULLPTR) {
        AuthCommon.safeRunNativeMethod(new Runnable() {
            @Override
            public void run() {
              nativeOnCodeSent(cListener, verificationId, token);
            }
          });
      }
    }
  }

  @Override
  public void onCodeAutoRetrievalTimeOut(final String verificationId) {
    synchronized (lock) {
      if (cListener != CPP_NULLPTR) {
        AuthCommon.safeRunNativeMethod(new Runnable() {
            @Override
            public void run() {
              nativeOnCodeAutoRetrievalTimeOut(cListener, verificationId);
            }
          });
      }
    }
  }

  /**
   * This function is implemented in the Auth C++ library (credential_android.cc).
   */
  private native void nativeOnVerificationCompleted(long cListener,
                                                    PhoneAuthCredential credential);
  private native void nativeOnVerificationFailed(long cListener, String exceptionMessage);
  private native void nativeOnCodeSent(long cListener, String verificationId,
                                       ForceResendingToken forceResendingToken);
  private native void nativeOnCodeAutoRetrievalTimeOut(long cListener, String verificationId);
}
