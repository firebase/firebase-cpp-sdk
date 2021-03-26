package com.google.firebase.firestore.internal.cpp;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Helper methods for working with {@link Task} objects from C++. */
public final class FirestoreTasks {

  private FirestoreTasks() {}

  /**
   * Blocks the calling thread until the given {@link Task} has completed.
   *
   * <p>This method is identical to {@link com.google.android.gms.tasks.Tasks#await} except that it
   * does <em>not</em> throw an exception if invoked from the main thread. Since it is technically
   * possible to wait for a {@link Task} from any thread in C++, throwing is undesirable if called
   * from the main thread.
   *
   * <p>The result of the given {@link Task} (i.e. success or failure) is not considered by this
   * method; whenever the task completes, either successfully or unsuccessfully, this method will
   * return.
   *
   * @param task The task whose completion to await.
   * @throws InterruptedException if waiting for the task to complete is interrupted.
   */
  public static <T> void awaitCompletion(Task<T> task) throws InterruptedException {
    CountDownLatch countDownLatch = new CountDownLatch(1);
    ExecutorService executor = Executors.newSingleThreadExecutor();
    try {
      task.addOnCompleteListener(executor, new CountDownOnCompleteListener<T>(countDownLatch));
      countDownLatch.await();
    } finally {
      executor.shutdown();
    }
  }

  private static final class CountDownOnCompleteListener<T> implements OnCompleteListener<T> {
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
