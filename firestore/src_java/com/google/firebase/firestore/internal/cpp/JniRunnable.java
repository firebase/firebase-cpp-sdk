package com.google.firebase.firestore.internal.cpp;

import android.os.Handler;
import android.os.Looper;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.TaskCompletionSource;

/** A {@link Runnable} whose {@link #run} method calls a native function. */
public final class JniRunnable implements Runnable {

  private final Object lock = new Object();
  private final ThreadLocal<Integer> currentThreadActiveRunCount = new ThreadLocalRunDepth();
  private int totalActiveRunCount;
  private long data;

  /**
   * Creates a new instance of this class.
   *
   * @param data Opaque data used by the native code; must not be zero.
   * @throws IllegalArgumentException if {@code data==0}.
   */
  public JniRunnable(long data) {
    if (data == 0) {
      throw new IllegalArgumentException(
          "data==0 is forbidden because 0 is reserved to indicate that we are detached from the"
              + " C++ function");
    }
    this.data = data;
  }

  /**
   * Invokes the C++ function encapsulated by this object.
   *
   * <p>If {@link #detach} has been invoked then this method does nothing and returns as if
   * successful.
   */
  @Override
  public void run() {
    long dataCopy;
    synchronized (lock) {
      if (data == 0) {
        return;
      }
      dataCopy = data;
      totalActiveRunCount++;
      currentThreadActiveRunCount.set(currentThreadActiveRunCount.get() + 1);
    }

    try {
      nativeRun(dataCopy);
    } finally {
      synchronized (lock) {
        currentThreadActiveRunCount.set(currentThreadActiveRunCount.get() - 1);
        totalActiveRunCount--;
        lock.notifyAll();
      }
    }
  }

  /**
   * Releases the reference to native data.
   *
   * <p>After this method returns, all future invocations of {@link #run} will do nothing and return
   * as if successful.
   *
   * <p>This method blocks until all invocations of the native function called from {@link #run}
   * complete; therefore, when this method returns it is safe to delete any data that would be
   * referenced by the native function.
   *
   * <p>This method may be safely invoked multiple times. Subsequent invocations have no side
   * effects but will still block while there are active invocations of the native function.
   *
   * @throws InterruptedException if waiting for completion of the native function invocations is
   *     interrupted.
   */
  public void detach() throws InterruptedException {
    synchronized (lock) {
      data = 0;

      // Wait for invocations of the native function to complete before returning. Do not consider
      // native function invocations made by the current thread, which would happen if the native
      // function called detach(), because that would cause this method to deadlock because the
      // total run count would never reach zero.
      while (totalActiveRunCount - currentThreadActiveRunCount.get() > 0) {
        lock.wait();
      }
    }
  }

  /**
   * Invokes {@link #run} on the main event thread.
   *
   * <p>If the calling thread is the main event thread then {@link #run} is called synchronously.
   *
   * @return A {@link Task} that will complete after {@link #run} returns; if {@link #run} throws an
   *     exception, then that exception will be set as the exception of the task.
   */
  Task<Void> runOnMainThread() {
    TaskRunnable runnable = new TaskRunnable();
    Looper mainLooper = Looper.getMainLooper();
    if (Thread.currentThread() == mainLooper.getThread()) {
      runnable.run();
    } else {
      Handler handler = new Handler(mainLooper);
      boolean postSucceeded = handler.post(runnable);
      if (!postSucceeded) {
        runnable.setException(new RuntimeException("Handler.post() returned false"));
      }
    }
    return runnable.getTask();
  }

  /**
   * Invokes {@link #run} on a newly-created thread.
   *
   * @return A {@link Task} that will complete after {@link #run} returns; if {@link #run} throws an
   *     exception, then that exception will be set as the exception of the task.
   */
  Task<Void> runOnNewThread() {
    TaskRunnable runnable = new TaskRunnable();
    new Thread(runnable).start();
    return runnable.getTask();
  }

  /**
   * Invokes the encapsulated C++ function.
   *
   * @param data The data that was specified to the constructor, or {@code 0} if {@link #detach} has
   *     been called, in which case the native function should do nothing and return immediately.
   */
  private static native void nativeRun(long data);

  private final class TaskRunnable implements Runnable {

    private final TaskCompletionSource<Void> taskCompletionSource = new TaskCompletionSource<>();

    @Override
    public void run() {
      try {
        JniRunnable.this.run();
      } catch (Exception e) {
        taskCompletionSource.trySetException(e);
      } finally {
        taskCompletionSource.trySetResult(null);
      }
    }

    Task<Void> getTask() {
      return taskCompletionSource.getTask();
    }

    void setException(Exception exception) {
      taskCompletionSource.setException(exception);
    }
  }

  private static final class ThreadLocalRunDepth extends ThreadLocal<Integer> {
    @Override
    protected Integer initialValue() {
      return 0;
    }
  }
}
