// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.firebase.iid;

import com.google.firebase.FirebaseApp;
import com.google.firebase.testing.cppsdk.FakeReporter;
import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import javax.annotation.concurrent.GuardedBy;

/**
 * Mock FirebaseInstanceId.
 */
public class FirebaseInstanceId {

  /**
   * If set, {@link throwExceptionOrBlockThreadIfEnabled} will throw an IOException or
   * IllegalStateException (from the constructor) with this message.
   */
  private static String exceptionErrorMessage = null;

  /** If set, all method calls will block until this is signalled. */
  @GuardedBy("threadStarted")
  private static CountDownLatch threadBlocker = null;

  /** Sempahore used to wait for a thread to start. */
  private static final Semaphore threadStarted = new Semaphore(0);

  /** Semaphore used to wait for the woken up thread to finish. */
  private static final Semaphore threadFinished = new Semaphore(0);

  /**
   * Set a message which will be used to throw an Exception from all method calls.
   * Clear the message by setting the value to null.
   */
  public static void setThrowExceptionMessage(String errorMessage) {
    exceptionErrorMessage = errorMessage;
  }

  /** Make all operations block indefinitely until this flag is cleared. */
  public static boolean setBlockingMethodCallsEnable(boolean enable) {
    boolean stateChanged = false;
    synchronized (threadStarted) {
      if ((enable && threadBlocker == null) || (!enable && threadBlocker != null)) {
        stateChanged = true;
      }
      if (enable && stateChanged) {
        threadBlocker = new CountDownLatch(1);
        threadStarted.drainPermits();
        threadFinished.drainPermits();
      }
    }
    if (stateChanged && !enable) {
      synchronized (threadStarted) {
        threadBlocker.countDown();
        threadBlocker = null;
      }
      try {
        boolean acquired = threadFinished.tryAcquire(1, 1, TimeUnit.SECONDS);
        if (!acquired) {
          return false;
        }
      } catch (InterruptedException e) {
        return false;
      }
    }
    return true;
  }

  /** Wait for a thread to start and wait on {@link threadBlocker}. */
  public static boolean waitForBlockedThread() {
    boolean acquired = false;
    try {
      acquired = threadStarted.tryAcquire(1, 1, TimeUnit.SECONDS);
    } catch (InterruptedException e) {
      return false;
    }
    return acquired;
  }

  /** Block the thread if enabled by {@link setBlockingMethodCallsEnable}. */
  private static void blockThreadIfEnabled() {
    threadStarted.release();
    try {
      CountDownLatch latch = null;
      synchronized (threadStarted) {
        latch = threadBlocker;
      }
      if (latch != null) {
        latch.await();
      }
    } catch (InterruptedException e) {
      return;
    }
  }

  /** Signal thread completion to continue execution in {@link setBlockingMethodCallsEnable}. */
  private static void signalThreadCompletion() {
    threadFinished.release();
  }

  /**
   * Throw an exception or block the thread if enabled by {@link setThrowExceptionMessage} or
   * {@link setBlockingMethodCallsEnabled} respectively.
   */
  private static void throwExceptionOrBlockThreadIfEnabled() throws IOException {
    if (exceptionErrorMessage != null) {
      throw new IOException(exceptionErrorMessage);
    }
    blockThreadIfEnabled();
  }

  // Fake interface below.

  private FirebaseInstanceId() {
    if (exceptionErrorMessage != null) {
      throw new IllegalStateException(exceptionErrorMessage);
    }
    FakeReporter.addReport("FirebaseInstanceId.construct");
  }

  public String getId() {
    try {
      blockThreadIfEnabled();
      FakeReporter.addReport("FirebaseInstanceId.getId");
    } finally {
      signalThreadCompletion();
    }
    return "FakeId";
  }

  public long getCreationTime() {
    try {
      blockThreadIfEnabled();
      FakeReporter.addReport("FirebaseInstanceId.getCreationTime");
    } finally {
      signalThreadCompletion();
    }
    return 1512000287000L; // 11/29/17 16:04:47
  }

  public void deleteInstanceId() throws IOException {
    try {
      throwExceptionOrBlockThreadIfEnabled();
      FakeReporter.addReport("FirebaseInstanceId.deleteId");
    } finally {
      signalThreadCompletion();
    }
  }

  public String getToken(String authorizedEntity, String scope) throws IOException {
    try {
      throwExceptionOrBlockThreadIfEnabled();
      FakeReporter.addReport("FirebaseInstanceId.getToken", authorizedEntity, scope);
    } finally {
      signalThreadCompletion();
    }
    return "FakeToken";
  }

  public void deleteToken(String authorizedEntity, String scope) throws IOException {
    try {
      throwExceptionOrBlockThreadIfEnabled();
      FakeReporter.addReport("FirebaseInstanceId.deleteToken", authorizedEntity, scope);
    } finally {
      signalThreadCompletion();
    }
  }

  public static synchronized FirebaseInstanceId getInstance(FirebaseApp app) {
    return new FirebaseInstanceId();
  }
}
