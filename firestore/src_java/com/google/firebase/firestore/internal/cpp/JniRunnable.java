package com.google.firebase.firestore.internal.cpp;

import android.os.Handler;
import android.os.Looper;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.TaskCompletionSource;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/** A {@link Runnable} whose {@link #run} method calls a native function. */
public final class JniRunnable implements Runnable {

  private final ReentrantReadWriteLock.ReadLock readLock;
  private final ReentrantReadWriteLock.WriteLock writeLock;

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
    ReentrantReadWriteLock lock = new ReentrantReadWriteLock(/* fair= */ true);
    readLock = lock.readLock();
    writeLock = lock.writeLock();
    this.data = data;
  }

  /**
   * Invokes the C++ method encapsulated by this object.
   *
   * <p>If {@link #detach} has been invoked then this method does nothing and returns as if
   * successful.
   *
   * <p>This method <em>will</em> block if there is a thread blocked in {@link #detach}; otherwise,
   * it will call the C++ function without blocking. This may even result in concurrent/parallel
   * calls to the C++ function if {@link #run} is invoked concurrently.
   */
  @Override
  public void run() {
    readLock.lock();
    try {
      nativeRun(data);
    } finally {
      readLock.unlock();
    }
  }

  /**
   * Releases the reference to native data.
   *
   * <p>After this method returns, all future invocations of {@link #run} will do nothing and return
   * as if successful.
   *
   * <p>This method <em>will</em> block if there are active invocations of {@link #run}. Once all
   * active invocations of {@link #run} have completed, then this method will proceed and return
   * nearly instantly. Any invocations of {@link #run} that occur while {@link #detach} is blocked
   * will also block, allowing the number of active invocations of {@link #run} to eventually reach
   * zero and allow this method to proceed.
   */
  public void detach() {
    writeLock.lock();
    try {
      data = 0;
    } finally {
      writeLock.unlock();
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
}
