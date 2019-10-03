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

package com.google.firebase.app.internal.cpp;

import com.google.android.gms.tasks.OnCanceledListener;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;

/** Implements ResultCallback by redirecting calls into C++ from Task. */
public class JniResultCallback<TResult> {
  /** Interface for Task objects that can have notification disabled while a callback is pending. */
  private interface Callback {
    /**
     * Register the callback with the Task. If the Task is already complete this will immediately
     * result in a completion callback.
     */
    public void register();

    /**
     * Remove this class from the callback.
     */
    public void disconnect();
  }

  // C++ pointers. Use `long` type since it is a 64-bit integer.
  private long callbackFn;
  private long callbackData;

  private Callback callbackHandler = null;

  public static final String TAG = "FirebaseCb";

  /**
   * This class is registered with a Task object as an OnSuccessListener, OnFailureListener, and
   * OnCanceledListener, redirecting completion status to the C++ method nativeOnResult.
   */
  private class TaskCallback<TResult>
      implements OnSuccessListener<TResult>, OnFailureListener, OnCanceledListener, Callback {
    private Task<TResult> task;
    private final Object lockObject = new Object();
    /**
     * Register with a Task instance to capture the completion callback.
     */
    public TaskCallback(Task<TResult> task) {
      this.task = task;
    }

    @Override
    public void onSuccess(TResult result) {
      synchronized (lockObject) {
        if (task != null) {
          onCompletion(result, true, false, null);
        }
        disconnect();
      }
    }

    @Override
    public void onFailure(Exception exception) {
      synchronized (lockObject) {
        if (task != null) {
          onCompletion(exception, false, false, exception.getMessage());
        }
        disconnect();
      }
    }

    @Override
    public void onCanceled() {
      synchronized (lockObject) {
        if (task != null) {
          cancel();
        }
        disconnect();
      }
    }

    @Override
    public void register() {
      task.addOnSuccessListener(this);
      task.addOnFailureListener(this);
      task.addOnCanceledListener(this);
    }

    @Override
    public void disconnect() {
      synchronized (lockObject) {
        task = null;
      }
    }
  }

  /**
   * Constructor is called via JNI from C++. The `long` values are actually pointers. `callbackFn`
   * is a function pointer that will be called with `callbackData`, as well as the result, when the
   * operation is complete.
   */
  public JniResultCallback(Task<TResult> task, long callbackFn, long callbackData) {
    initializeNativeCallbackFunctionAndData(callbackFn, callbackData);
    initializeWithTask(task);
  }

  protected JniResultCallback(long callbackFn, long callbackData) {
    initializeNativeCallbackFunctionAndData(callbackFn, callbackData);
  }

  /**
   * Initialize the native callback function and data pointers.
   */
  protected void initializeNativeCallbackFunctionAndData(long callbackFn, long callbackData) {
    this.callbackFn = callbackFn;
    this.callbackData = callbackData;
  }

  /** Initialize / attach the instance to a pending result or task object. */
  protected void initializeWithTask(Task<TResult> task) {
    synchronized (this) {
      callbackHandler = new TaskCallback<TResult>(task);
      callbackHandler.register();
    }
  }

  /**
   * Cancel the callback by calling it with a failure status.
   */
  public void cancel() {
    // Complete the native callback to free data associated with callbackData.
    onCompletion(null, false, true, "cancelled");
  }

  /** Call nativeOnResult with the registered callbackFn and callbackData. */
  public void onCompletion(
      Object result, boolean success, boolean cancelled, String statusMessage) {
    synchronized (this) {
      if (callbackHandler != null) {
        nativeOnResult(result, success, cancelled, statusMessage, callbackFn, callbackData);
        callbackHandler.disconnect();
        callbackHandler = null;
      }
    }
  }

  /** This function is implemented in the App C++ library (util_android.cc). */
  private native void nativeOnResult(
      Object result,
      boolean success,
      boolean cancelled,
      String statusString,
      long callbackFn,
      long callbackData);
}
