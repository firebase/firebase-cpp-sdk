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
import com.google.firebase.auth.FirebaseAuthException;
import com.google.firebase.auth.PhoneAuthCredential;
import com.google.firebase.auth.PhoneAuthProvider.ForceResendingToken;
import com.google.firebase.auth.PhoneAuthProvider.OnVerificationStateChangedCallbacks;
import java.util.ArrayList;

/**
 * Redirect PhoneAuthProvider callbacks into C++.
 */
public class JniAuthPhoneListener extends OnVerificationStateChangedCallbacks {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Native callback context handle cleared by disconnect().
  // Synchronized on `this` along with activeCallbackThreads.
  private long cListener;
  private final ArrayList<Thread> activeCallbackThreads = new ArrayList<>();

  public JniAuthPhoneListener(long cListener) {
    this.cListener = cListener;
  }

  private boolean hasOtherActiveThreads(Thread currentThread) {
    for (int i = 0; i < activeCallbackThreads.size(); i++) {
      if (activeCallbackThreads.get(i) != currentThread) {
        return true;
      }
    }
    return false;
  }

  public synchronized void disconnect() {
    cListener = CPP_NULLPTR;
    Thread currentThread = Thread.currentThread();
    boolean interrupted = false;
    while (hasOtherActiveThreads(currentThread)) {
      try {
        wait();
      } catch (InterruptedException e) {
        interrupted = true;
      }
    }
    if (interrupted) {
      currentThread.interrupt();
    }
  }

  private synchronized long beginCallback() {
    if (cListener == CPP_NULLPTR) {
      return CPP_NULLPTR;
    }
    activeCallbackThreads.add(Thread.currentThread());
    return cListener;
  }

  private synchronized void endCallback() {
    activeCallbackThreads.remove(Thread.currentThread());
    notifyAll();
  }

  @Override
  public void onVerificationCompleted(final PhoneAuthCredential credential) {
    final long listenerPtr = beginCallback();
    if (listenerPtr != CPP_NULLPTR) {
      try {
        AuthCommon.safeRunNativeMethod(new Runnable() {
          @Override
          public void run() {
            nativeOnVerificationCompleted(listenerPtr, credential);
          }
        });
      } finally {
        endCallback();
      }
    }
  }

  @Override
  public void onVerificationFailed(final FirebaseException exception) {
    final long listenerPtr = beginCallback();
    if (listenerPtr != CPP_NULLPTR) {
      try {
        String message = (exception != null) ? exception.getMessage() : null;
        if (exception instanceof FirebaseAuthException) {
          String errorCode = ((FirebaseAuthException) exception).getErrorCode();
          if (errorCode != null && !errorCode.isEmpty()) {
            message = (message != null && !message.isEmpty())
                ? (errorCode + ": " + message)
                : errorCode;
          }
        }
        final String safeMessage =
            (message != null) ? message : "Phone verification failed.";
        AuthCommon.safeRunNativeMethod(new Runnable() {
          @Override
          public void run() {
            nativeOnVerificationFailed(listenerPtr, safeMessage);
          }
        });
      } finally {
        endCallback();
      }
    }
  }

  @Override
  public void onCodeSent(final String verificationId, final ForceResendingToken token) {
    final long listenerPtr = beginCallback();
    if (listenerPtr != CPP_NULLPTR) {
      try {
        final String safeVerificationId = (verificationId != null) ? verificationId : "";
        AuthCommon.safeRunNativeMethod(new Runnable() {
          @Override
          public void run() {
            nativeOnCodeSent(listenerPtr, safeVerificationId, token);
          }
        });
      } finally {
        endCallback();
      }
    }
  }

  @Override
  public void onCodeAutoRetrievalTimeOut(final String verificationId) {
    final long listenerPtr = beginCallback();
    if (listenerPtr != CPP_NULLPTR) {
      try {
        final String safeVerificationId = (verificationId != null) ? verificationId : "";
        AuthCommon.safeRunNativeMethod(new Runnable() {
          @Override
          public void run() {
            nativeOnCodeAutoRetrievalTimeOut(listenerPtr, safeVerificationId);
          }
        });
      } finally {
        endCallback();
      }
    }
  }

  /**
   * This function is implemented in the Auth C++ library (credential_android.cc).
   */
  private native void nativeOnVerificationCompleted(long cListener, PhoneAuthCredential credential);
  private native void nativeOnVerificationFailed(long cListener, String exceptionMessage);
  private native void nativeOnCodeSent(
      long cListener, String verificationId, ForceResendingToken forceResendingToken);
  private native void nativeOnCodeAutoRetrievalTimeOut(long cListener, String verificationId);
}
