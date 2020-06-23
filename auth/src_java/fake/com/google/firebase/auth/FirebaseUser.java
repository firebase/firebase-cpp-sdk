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
import com.google.android.gms.tasks.Task;
import com.google.firebase.testing.cppsdk.ConfigAndroid;
import com.google.firebase.testing.cppsdk.ConfigRow;
import com.google.firebase.testing.cppsdk.FakeListener;
import com.google.firebase.testing.cppsdk.TickerAndroid;
import java.util.List;

/** Fake FirebaseUser (not completed yet). */
public final class FirebaseUser extends UserInfo {
  public boolean isAnonymous() {
    return true;
  }

  public Task<GetTokenResult> getIdToken(boolean forceRefresh) {
    Task<GetTokenResult> result = Task.forResult("FirebaseUser.getIdToken", new GetTokenResult());
    TickerAndroid.register(result);
    return result;
  }

  public List<? extends UserInfo> getProviderData() {
    return null;
  }

  public Task<Void> updateEmail(String email) {
    final String configKey = "FirebaseUser.updateEmail";
    Task<Void> result = Task.forResult(configKey, null);

    ConfigRow row = ConfigAndroid.get(configKey);
    if (!row.futuregeneric().throwexception()) {
      result.addListener(
          new FakeListener<Void>() {
            @Override
            public void onSuccess(Void res) {
              FirebaseUser.this.email = email;
            }
          });
    }

    TickerAndroid.register(result);
    return result;
  }

  public Task<Void> updatePassword(String email) {
    Task<Void> result = Task.forResult("FirebaseUser.updatePassword", null);
    TickerAndroid.register(result);
    return result;
  }

  public Task<Void> updateProfile(UserProfileChangeRequest request) {
    Task<Void> result = Task.forResult("FirebaseUser.updateProfile", null);
    TickerAndroid.register(result);
    return result;
  }

  public Task<AuthResult> linkWithCredential(AuthCredential credential) {
    Task<AuthResult> result = Task.forResult("FirebaseUser.linkWithCredential", new AuthResult());
    TickerAndroid.register(result);
    return result;
  }

  /**
   * Links the user using the mobile browser (either a Custom Chrome Tab or the device's default
   * browser) to the given {@code provider}. If the calling activity dies during this operation, use
   * {@link FirebaseAuth#getPendingAuthResult()} to get the outcome of this operation.
   *
   * <p>Note: this call has a UI associated with it, unlike the majority of calls in FirebaseAuth.
   *
   * <h5>Exceptions</h5>
   *
   * <ul>
   *   <li>{@link FirebaseAuthInvalidCredentialsException} thrown if the credential generated from
   *       the flow is malformed or expired.
   *   <li>{@link FirebaseAuthInvalidUserException} thrown if the user has been disabled by an
   *       administrator.
   *   <li>{@link FirebaseAuthUserCollisionException} thrown if the email that keys the user that is
   *       signing in is already in use.
   *   <li>{@link FirebaseAuthWebException} thrown if there is an operation already in progress, the
   *       pending operation was canceled, there is a problem with 3rd party cookies in the browser,
   *       or some other error in the web context has occurred.
   *   <li>{@link FirebaseAuthException} thrown if signing in via this method has been disabled in
   *       the Firebase Console, or if the {@code provider} passed is configured improperly.
   * </ul>
   *
   * @param activity the current {@link Activity} that you intent to launch this flow from
   * @param federatedAuthProvider an {@link FederatedAuthProvider} configured with information about
   *     the provider that you intend to link to the user.
   * @return a {@link Task} with a reference to an {@link AuthResult} with user information upon
   *     success
   */
  public Task<AuthResult> startActivityForLinkWithProvider(
      Activity activity, FederatedAuthProvider federatedAuthProvider) {
    Task<AuthResult> result =
        Task.forResult("FirebaseUser.startActivityForLinkWithProvider", new AuthResult());
    TickerAndroid.register(result);
    return result;
  }

  public Task<AuthResult> unlink(String provider) {
    Task<AuthResult> result = Task.forResult("FirebaseUser.unlink", new AuthResult());
    TickerAndroid.register(result);
    return result;
  }

  public Task<Void> updatePhoneNumber(PhoneAuthCredential credential) {
    return null;
  }

  public Task<Void> reload() {
    Task<Void> result = Task.forResult("FirebaseUser.reload", null);
    TickerAndroid.register(result);
    return result;
  }

  public Task<Void> reauthenticate(AuthCredential credential) {
    Task<Void> result = Task.forResult("FirebaseUser.reauthenticate", null);
    TickerAndroid.register(result);
    return result;
  }

  public Task<AuthResult> reauthenticateAndRetrieveData(AuthCredential credential) {
    Task<AuthResult> result =
        Task.forResult("FirebaseUser.reauthenticateAndRetrieveData", new AuthResult());
    TickerAndroid.register(result);
    return result;
  }

  /**
   * Reauthenticates the user using the mobile browser (either a Custom Chrome Tab or the device's
   * default browser) using the given {@code provider}. If the calling activity dies during this
   * operation, use {@link FirebaseAuth#getPendingAuthResult()} to get the outcome of this
   * operation.
   *
   * <p>Note: this call has a UI associated with it, unlike the majority of calls in FirebaseAuth.
   *
   * <h5>Exceptions</h5>
   *
   * <ul>
   *   <li>{@link FirebaseAuthInvalidCredentialsException} thrown if the credential generated from
   *       the flow is malformed or expired.
   *   <li>{@link FirebaseAuthInvalidUserException} thrown if the user has been disabled by an
   *       administrator.
   *   <li>{@link FirebaseAuthUserCollisionException} thrown if the email that keys the user that is
   *       signing in is already in use.
   *   <li>{@link FirebaseAuthWebException} thrown if there is an operation already in progress, the
   *       pending operation was canceled, there is a problem with 3rd party cookies in the browser,
   *       or some other error in the web context has occurred.
   *   <li>{@link FirebaseAuthException} thrown if signing in via this method has been disabled in
   *       the Firebase Console, or if the {@code provider} passed is configured improperly.
   * </ul>
   *
   * @param activity the current {@link Activity} that you intent to launch this flow from
   * @param federatedAuthProvider an {@link FederatedAuthProvider} configured with information about
   *     how you intend the user to reauthenticate.
   * @return a {@link Task} with a reference to an {@link AuthResult} with user information upon
   *     success
   */
  public Task<AuthResult> startActivityForReauthenticateWithProvider(
      Activity activity, FederatedAuthProvider federatedAuthProvider) {
    Task<AuthResult> result =
        Task.forResult("FirebaseUser.startActivityForReauthenticateWithProvider", new AuthResult());
    TickerAndroid.register(result);
    return result;
  }


  public Task<Void> delete() {
    Task<Void> result = Task.forResult("FirebaseUser.delete", null);
    TickerAndroid.register(result);
    return result;
  }

  public Task<Void> sendEmailVerification() {
    Task<Void> result = Task.forResult("FirebaseUser.sendEmailVerification", null);
    TickerAndroid.register(result);
    return result;
  }

  /** Returns the {@link FirebaseUserMetadata} associated with this user. */
  public FirebaseUserMetadata getMetadata() {
    return new FirebaseUserMetadata();
  }
}
