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
import com.google.firebase.FirebaseApiNotAvailableException;
import com.google.firebase.FirebaseApp;
import com.google.firebase.FirebaseException;
import com.google.firebase.FirebaseNetworkException;
import com.google.firebase.FirebaseTooManyRequestsException;
import com.google.firebase.testing.cppsdk.ConfigAndroid;
import com.google.firebase.testing.cppsdk.ConfigRow;
import com.google.firebase.testing.cppsdk.FakeListener;
import com.google.firebase.testing.cppsdk.TickerAndroid;
import java.util.ArrayList;
import java.util.Random;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Fake FirebaseAuth */
public final class FirebaseAuth {
  // This makes the signed-in status consistent and thus makes setting up test data config easier.
  private boolean signedIn = false;

  // Random number generator for listener callback delays.
  private final Random randomDelay = new Random();

  private final ArrayList<AuthStateListener> authStateListeners = new ArrayList<>();
  private final ArrayList<IdTokenListener> idTokenListeners = new ArrayList<>();

  public static FirebaseAuth getInstance(FirebaseApp firebaseApp) {
    return new FirebaseAuth();
  }

  public FirebaseUser getCurrentUser() {
    if (signedIn) {
      return new FirebaseUser();
    } else {
      return null;
    }
  }

  public static <T> Task<T> applyAuthExceptionFromConfig(Task<T> task, String exceptionMsg) {
    Pattern r = Pattern.compile("^\\[(.*)[?!:]:?(.*)\\] (.*)");
    Matcher matcher = r.matcher(exceptionMsg);
    if (matcher.find()) {
      String exceptionName = matcher.group(1);
      String errorCode = matcher.group(2);
      String errorMessage = matcher.group(3);
      if (exceptionName.equals("FirebaseAuthInvalidCredentialsException")) {
        task.setException(new FirebaseAuthInvalidCredentialsException(errorCode, errorMessage));
      } else if (exceptionName.equals("FirebaseAuthActionCodeException")) {
        task.setException(new FirebaseAuthActionCodeException(errorCode, errorMessage));
      } else if (exceptionName.equals("FirebaseAuthEmailException")) {
        task.setException(new FirebaseAuthEmailException(errorCode, errorMessage));
      } else if (exceptionName.equals("FirebaseAuthException")) {
        task.setException(new FirebaseAuthException(errorCode, errorMessage));
      } else if (exceptionName.equals("FirebaseAuthInvalidUserException")) {
        task.setException(new FirebaseAuthInvalidUserException(errorCode, errorMessage));
      } else if (exceptionName.equals("FirebaseAuthRecentLoginRequiredException")) {
        task.setException(new FirebaseAuthRecentLoginRequiredException(errorCode, errorMessage));
      } else if (exceptionName.equals("FirebaseAuthUserCollisionException")) {
        task.setException(new FirebaseAuthUserCollisionException(errorCode, errorMessage));
      } else if (exceptionName.equals("FirebaseAuthWeakPasswordException")) {
        task.setException(new FirebaseAuthWeakPasswordException(errorCode, errorMessage));
      } else if (exceptionName.equals("FirebaseApiNotAvailableException")) {
        task.setException(new FirebaseApiNotAvailableException(errorMessage));
      } else if (exceptionName.equals("FirebaseException")) {
        task.setException(new FirebaseException(errorMessage));
      } else if (exceptionName.equals("FirebaseNetworkException")) {
        task.setException(new FirebaseNetworkException(errorMessage));
      } else if (exceptionName.equals("FirebaseTooManyRequestsException")) {
        task.setException(new FirebaseTooManyRequestsException(errorMessage));
      }
    }
    return task;
  }

  /**
   * Delay the calling thread between 0..100ms.
   */
  private void randomDelayThread() {
    try {
      Thread.sleep(randomDelay.nextInt(100));
    } catch (InterruptedException e) {
      // ignore
    }
  }

  public void addAuthStateListener(final AuthStateListener listener) {
    authStateListeners.add(listener);
    new Thread(
            new Runnable() {
              @Override
              public void run() {
                randomDelayThread();
                listener.onAuthStateChanged(FirebaseAuth.this);
              }
            })
        .start();
  }

  public void removeAuthStateListener(AuthStateListener listener) {
    authStateListeners.remove(listener);
  }

  public void addIdTokenListener(final IdTokenListener listener) {
    idTokenListeners.add(listener);
    new Thread(
            new Runnable() {
              @Override
              public void run() {
                randomDelayThread();
                listener.onIdTokenChanged(FirebaseAuth.this);
              }
            })
        .start();
  }

  public void removeIdTokenListener(IdTokenListener listener) {
    idTokenListeners.remove(listener);
  }

  public void signOut() {
    signedIn = false;
  }

  public Task<SignInMethodQueryResult> fetchSignInMethodsForEmail(String email) {
    return null;
  }

  /** A generic helper function to mimic all types of sign-in actions. */
  private Task<AuthResult> signInHelper(String configKey) {
    Task<AuthResult> result = Task.forResult(configKey, new AuthResult());

    ConfigRow row = ConfigAndroid.get(configKey);
    if (!row.futuregeneric().throwexception()) {
      result.addListener(new FakeListener<AuthResult>() {
        @Override
        public void onSuccess(AuthResult res) {
          signedIn = true;
          for (AuthStateListener listener : authStateListeners) {
            listener.onAuthStateChanged(FirebaseAuth.this);
          }
        }
      });
    } else {
      result = applyAuthExceptionFromConfig(result, row.futuregeneric().exceptionmsg());
    }

    TickerAndroid.register(result);
    return result;
  }

  public Task<AuthResult> signInWithCustomToken(String token) {
    return signInHelper("FirebaseAuth.signInWithCustomToken");
  }

  public Task<AuthResult> signInWithCredential(AuthCredential credential) {
    return signInHelper("FirebaseAuth.signInWithCredential");
  }

  public Task<AuthResult> signInAnonymously() {
    return signInHelper("FirebaseAuth.signInAnonymously");
  }

  public Task<AuthResult> signInWithEmailAndPassword(String email, String password) {
    return signInHelper("FirebaseAuth.signInWithEmailAndPassword");
  }

  /**
   * Signs in the user using the mobile browser (either a Custom Chrome Tab or the device's default
   * browser) for the given {@code provider}.
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
   * @param activity the current {@link Activity} from which you intend to launch this flow.
   * @param federatedAuthProvider an {@link FederatedAuthProvider} configured with information about
   *     how you intend the user to sign in.
   * @return a {@link Task} with a reference to an {@link AuthResult} with user information upon
   *     success
   */
  public Task<AuthResult> startActivityForSignInWithProvider(
      Activity activity, FederatedAuthProvider federatedAuthProvider) {
    return signInHelper("FirebaseAuth.startActivityForSignInWithProvider");
  }

  public Task<AuthResult> createUserWithEmailAndPassword(String email, String password) {
    return signInHelper("FirebaseAuth.createUserWithEmailAndPassword");
  }

  public Task<Void> sendPasswordResetEmail(String email) {
    Task<Void> result = Task.forResult("FirebaseAuth.sendPasswordResetEmail", null);
    ConfigRow row = ConfigAndroid.get("FirebaseAuth.sendPasswordResetEmail");
    if (row.futuregeneric().throwexception()) {
      result = applyAuthExceptionFromConfig(result, row.futuregeneric().exceptionmsg());
    }
    TickerAndroid.register(result);
    return result;
  }

  /** AuthStateListener */
  public interface AuthStateListener {
    void onAuthStateChanged(FirebaseAuth auth);
  }

  /** IdTokenListener */
  public interface IdTokenListener {
    void onIdTokenChanged(FirebaseAuth auth);
  }
}
