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

package com.google.firebase.auth;

import android.app.Activity;
import com.google.firebase.FirebaseException;
import java.util.concurrent.TimeUnit;

/** Fake PhoneAuthProvider */
public class PhoneAuthProvider {

  /** Fake OnVerificationStateChangedCallbacks */
  public abstract static class OnVerificationStateChangedCallbacks {
    public abstract void onVerificationCompleted(PhoneAuthCredential credential);

    public abstract void onVerificationFailed(FirebaseException exception);

    public void onCodeSent(String verificationId, ForceResendingToken forceResendingToken) {}

    public void onCodeAutoRetrievalTimeOut(String verificationId) {}
  }

  /** Fake ForceResendingToken */
  public static class ForceResendingToken {}

  public static PhoneAuthProvider getInstance(FirebaseAuth firebaseAuth) {
    return null;
  }

  public static PhoneAuthCredential getCredential(
      String verificationId, String smsCode) {
    return null;
  }

  public void verifyPhoneNumber(
      String phoneNumber,
      long timeout,
      TimeUnit unit,
      Activity activity,
      OnVerificationStateChangedCallbacks callbacks,
      ForceResendingToken forceResendingToken) {}
}
