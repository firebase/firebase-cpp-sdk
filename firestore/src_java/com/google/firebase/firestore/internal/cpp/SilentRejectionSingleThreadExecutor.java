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
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

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
  private final ExecutorService internalExecutor =
      Executors.newSingleThreadExecutor();

  @Override
  public void execute(Runnable command) {
    try {
      internalExecutor.execute(command);
    } catch (RejectedExecutionException e) {
      // Swallow RejectedExecutionException
    }
  }

  public void shutdown() {
    // Do not run any tasks that are already awaiting.
    internalExecutor.shutdownNow();
  }
}
