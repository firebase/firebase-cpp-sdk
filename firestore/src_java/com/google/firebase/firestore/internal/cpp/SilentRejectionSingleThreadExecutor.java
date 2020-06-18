package com.google.firebase.firestore.internal.cpp;

import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

/**
 * Simple {@code Executor} implementation wraps around a single threaded executor and swallows
 * {@code RejectedExecutionException} when executing commands.
 *
 * <p>During shutdown, the C++ API must be able to prevent user callbacks from running after the
 * Firestore object has been disposed. To do so, it shuts down its executors, accepting that new
 * callbacks may be rejected. This class catches and discards the {@code RejectedExecutionException}
 * that is thrown by the underlying Java executor after shutdown, bridging the gap between C++
 * expectations and the Java implementation.
 */
public final class SilentRejectionSingleThreadExecutor implements Executor {
  private final ExecutorService internalExecutor = Executors.newSingleThreadExecutor();

  @Override
  public void execute(Runnable command) {
    try {
      internalExecutor.execute(command);
    } catch (RejectedExecutionException e) {
      // Swallow RejectedExecutionException
    }
  }

  public void shutdown() {
    internalExecutor.shutdown();
  }
}
