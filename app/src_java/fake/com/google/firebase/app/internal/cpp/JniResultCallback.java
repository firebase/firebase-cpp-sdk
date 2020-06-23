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

package com.google.firebase.app.internal.cpp;

import com.google.android.gms.tasks.Task;
import com.google.firebase.testing.cppsdk.FakeListener;

/**
 * Fake firebase/app/client/cpp/src_java/com/google/firebase/app/internal/cpp/JniResultCallback.java
 */
public class JniResultCallback<TResult> {
  private interface Callback {
    public void register();

    public void disconnect();
  };

  private long callbackFn;
  private long callbackData;
  private Callback callbackHandler = null;

  public static final String LOG_TAG = "FakeFirebaseCb";

  private class TaskCallback<T> extends FakeListener<T> implements Callback {
    private Task<T> task;

    public TaskCallback(Task<T> task) {
      this.task = task;
    }

    @Override
    public void onSuccess(T result) {
      if (task != null) {
        onCompletion(result, true, false, null);
      }
      disconnect();
    }

    @Override
    public void onFailure(Exception exception) {
      if (task != null) {
        onCompletion(exception, false, false, exception.getMessage());
      }
      disconnect();
    }

    @Override
    public void register() {
      task.addListener(this);
    }

    @Override
    public void disconnect() {
      task = null;
    }
  }

  @SuppressWarnings("unchecked")
  public JniResultCallback(Task<TResult> task, long callbackFn, long callbackData) {
    Log.i(LOG_TAG, String.format("JniResultCallback: Fn %x, Data %x", callbackFn, callbackData));
    this.callbackFn = callbackFn;
    this.callbackData = callbackData;
    callbackHandler = new TaskCallback<>(task);
    callbackHandler.register();
  }

  public void cancel() {
    Log.i(LOG_TAG, "canceled");
    onCompletion(null, false, true, "cancelled (fake)");
  }

  private void onCompletion(
      Object result, boolean success, boolean cancelled, String statusMessage) {
    if (callbackHandler != null) {
      nativeOnResult(
          result, success, cancelled, statusMessage, callbackFn, callbackData);
      callbackHandler.disconnect();
      callbackHandler = null;
    }
  }

  private native void nativeOnResult(
      Object result,
      boolean success,
      boolean cancelled,
      String statusString,
      long callbackFn,
      long callbackData);
}
