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

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/** Context for a native C++ function on a alternate thread. */
public class CppThreadDispatcherContext {
  /** C++ function to execute on a thread. */
  private long functionPtr;
  /** C++ function to execute on cancelation. */
  private long cancelFunctionPtr;
  /** Data to pass to either C++ function on the thread. */
  private long functionData;
  /** Controls access to this object. */
  private final Lock lock = new ReentrantLock();

  /** Create the context for the thread being dispatched. */
  public CppThreadDispatcherContext(long functionPtr, long functionData, long cancelFunctionPtr) {
    this.functionPtr = functionPtr;
    this.functionData = functionData;
    this.cancelFunctionPtr = cancelFunctionPtr;
  }

  /** Clear the C++ methods referenced by this instance. */
  private void clear() {
    try {
      lock.lock();
      functionPtr = 0;
      functionData = 0;
      cancelFunctionPtr = 0;
    } finally {
      lock.unlock();
    }
  }

  /** Cancel the C++ method from the dispatch thread. */
  public void cancel() {
    try {
      lock.lock();
      if (cancelFunctionPtr != 0) {
        nativeFunction(cancelFunctionPtr, functionData);
      }
    } finally {
      clear();
      lock.unlock();
    }
  }

  /** Execute the C++ method (if any) associated with this object. */
  public void execute() {
    try {
      lock.lock();
      if (functionPtr != 0) {
        nativeFunction(functionPtr, functionData);
      }
    } finally {
      clear();
      lock.unlock();
    }
  }

  /**
   * Release the execution / cancelation lock to allow the current operation to be cancelled.
   *
   * Before executing a long running operation that can be canceled from nativeFunction(), the
   * C/C++ method should call releaseExecuteCancelLock(). When the long running operation is
   * complete, acquireExecuteCancelLock() can be used to re-acquire the execution lock and determine
   * whether the C/C++ state is still valid.
   */
  public void releaseExecuteCancelLock() {
    lock.unlock();
  }

  /**
   * Acquire the execution / cancelation lock and return whether the object still has an execution
   * function associated with it.
   *
   * If this method returns false, no execution function is present.  If no execution function is
   * present, execution is complete or the operation was cancelled.  If the operation is complete
   * or cancelled, the C/C++ state may be invalid depending upon the side effects of the
   * execution or cancellation method.
   * The lock is always acquired.
   */
  public boolean acquireExecuteCancelLock() {
    lock.lock();
    return functionPtr != 0;
  }

  private static native void nativeFunction(long functionPointer, long functionData);
}
