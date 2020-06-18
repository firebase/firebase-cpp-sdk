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

package com.google.android.gms.tasks;

import com.google.firebase.testing.cppsdk.ConfigAndroid;
import com.google.firebase.testing.cppsdk.ConfigRow;
import com.google.firebase.testing.cppsdk.FakeListener;
import com.google.firebase.testing.cppsdk.TickerAndroid;
import com.google.firebase.testing.cppsdk.TickerObserver;
import java.util.Vector;

/**
 * Fake Task class that accepts instruction from {@link setEta}, {@link setResult} and {@link
 * setException} and acts accordingly.
 */
public class Task<TResult> implements TickerObserver {
  private final Vector<FakeListener<? super TResult>> mListenerQueue = new Vector<>();
  private long eta;
  private TResult mResult;
  private Exception mException;

  public TResult getResult() throws Exception {
    if (mException != null) {
      throw mException;
    }
    return mResult;
  }

  @Override
  public void elapse() {
    if (isComplete() && !mListenerQueue.isEmpty()) {
      for (FakeListener<? super TResult> listener : mListenerQueue) {
        if (mException == null) {
          listener.onSuccess(mResult);
        } else {
          listener.onFailure(mException);
        }
      }
      mListenerQueue.clear();
    }
  }

  public boolean isComplete() {
    return eta <= TickerAndroid.now();
  }

  public boolean isSuccessful() {
    return isComplete() && mException == null;
  }

  public void setEta(long eta) {
    this.eta = eta;
  }

  /** Set what result the task should return unless you also call {@link setException}. */
  public void setResult(TResult result) {
    mResult = result;
  }

  /** Set an exception the task should throw. */
  public void setException(Exception e) {
    mException = e;
  }

  /**
   * To make writing fake less cumbersome, we use a single type of {@link FakeListener} to mimic all
   * types of listeners.
   */
  public Task<TResult> addListener(FakeListener<? super TResult> listener) {
    mListenerQueue.add(listener);
    elapse();
    return this;
  }

  /** A helper function to get a task that returns immediately the specified result. */
  public static <TResult> Task<TResult> forResult(TResult result) {
    Task<TResult> task = new Task<>();
    task.setResult(result);
    task.setEta(0L);
    return task;
  }

  /** A helper function to get a task from a {@link ConfigRow}. */
  public static <TResult> Task<TResult> forResult(String configKey, TResult result) {
    ConfigRow row = ConfigAndroid.get(configKey);
    if (row == null) {
      // Default behavior when no config is set.
      return forResult(result);
    }

    Task<TResult> task = new Task<>();
    if (row.futuregeneric().throwexception()) {
      task.setException(new Exception(row.futuregeneric().exceptionmsg()));
    } else {
      task.setResult(result);
    }
    task.setEta(row.futuregeneric().ticker());
    return task;
  }
}
