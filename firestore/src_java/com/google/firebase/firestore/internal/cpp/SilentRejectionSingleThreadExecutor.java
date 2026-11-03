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

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * Simple {@code Executor} implementation wraps around a single threaded
 * executor and swallows
 * {@code RejectedExecutionException} when executing commands.
 *
 * <p>During shutdown, the C++ API must be able to prevent user callbacks from
 * running after the Firestore object has been disposed. To do so, it shuts down
 * its executors, accepting that new callbacks may be rejected, and if there is
 * work already scheduled, they will not be executed.
 *
 * This class catches and discards the {@code RejectedExecutionException}
 * that is thrown by the underlying Java executor after shutdown, bridging the
 * gap between C++ expectations and the Java implementation.
 */
public final class SilentRejectionSingleThreadExecutor implements Executor {
  private volatile Thread executorThread;
  private final ThreadPoolExecutor internalExecutor =
      new ThreadPoolExecutor(
          1,
          1,
          0L,
          TimeUnit.MILLISECONDS,
          new LinkedBlockingQueue<Runnable>(),
          new ThreadFactory() {
            private final ThreadFactory defaultFactory =
                Executors.defaultThreadFactory();

            @Override
            public Thread newThread(Runnable r) {
              Thread thread = defaultFactory.newThread(r);
              thread.setDaemon(true);
              executorThread = thread;
              return thread;
            }
          });

  @Override
  public synchronized void execute(Runnable command) {
    if (command == null) {
      return;
    }
    try {
      internalExecutor.execute(command);
    } catch (RejectedExecutionException e) {
      // Swallow RejectedExecutionException
    }
  }

  public void shutdown() {
    synchronized (this) {
      // Do not run any tasks that are already awaiting, but do not interrupt
      // an in-flight task that may be executing JNI or user code.
      internalExecutor.getQueue().clear();
      internalExecutor.shutdown();
    }
    if (Thread.currentThread() != executorThread) {
      boolean interrupted = false;
      while (!internalExecutor.isTerminated()) {
        try {
          internalExecutor.awaitTermination(1, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
          interrupted = true;
        }
      }
      if (interrupted) {
        Thread.currentThread().interrupt();
      }
    }
  }
}
