/*
 * Copyright 2021 Google LLC
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

package com.google.firebase.firestore.internal.cpp;

import com.google.android.gms.tasks.Continuation;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.firebase.firestore.FirebaseFirestoreException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Helper methods for working with {@link Task} objects from C++.
 */
public final class FirestoreTasks {
  private FirestoreTasks() {}

  /**
   * Takes a {@link Task} and returns a new Task, whose result will be the same
   * as the incoming Task, if the result is not null, otherwise fails the
   * returned Task with given error message.
   */
  public static <T> Task<T> failTaskWhenResultIsNull(
      Task<T> task, final String message) {
    Continuation<T, T> continuation = new Continuation<T, T>() {
      @Override
      public T then(Task<T> task) throws Exception {
        T result = task.getResult();
        if (result == null) {
          throw new FirebaseFirestoreException(
              message, FirebaseFirestoreException.Code.NOT_FOUND);
        }
        return result;
      }
    };
    return task.continueWith(continuation);
  }

  /**
   * Blocks the calling thread until the given {@link Task} has completed.
   *
   * <p>This method is identical to {@link
   * com.google.android.gms.tasks.Tasks#await} except that it does <em>not</em>
   * throw an exception if invoked from the main thread. Since it is technically
   * possible to wait for a {@link Task} from any thread in C++, throwing is
   * undesirable if called from the main thread.
   *
   * <p>The result of the given {@link Task} (i.e. success or failure) is not
   * considered by this method; whenever the task completes, either successfully
   * or unsuccessfully, this method will return.
   *
   * @param task The task whose completion to await.
   * @throws InterruptedException if waiting for the task to complete is
   *     interrupted.
   */
  public static <T> void awaitCompletion(Task<T> task)
      throws InterruptedException {
    CountDownLatch countDownLatch = new CountDownLatch(1);
    ExecutorService executor = Executors.newSingleThreadExecutor();
    try {
      task.addOnCompleteListener(
          executor, new CountDownOnCompleteListener<T>(countDownLatch));
      countDownLatch.await();
    } finally {
      executor.shutdown();
    }
  }

  private static final class CountDownOnCompleteListener<T>
      implements OnCompleteListener<T> {
    private final CountDownLatch countDownLatch;

    CountDownOnCompleteListener(CountDownLatch countDownLatch) {
      this.countDownLatch = countDownLatch;
    }

    @Override
    public void onComplete(Task<T> task) {
      countDownLatch.countDown();
    }
  }
}
