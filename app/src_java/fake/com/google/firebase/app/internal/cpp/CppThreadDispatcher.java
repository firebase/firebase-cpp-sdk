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

import android.app.Activity;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Runs a native C++ function on an alternate thread. */
public class CppThreadDispatcher {
  private static final ExecutorService executor = Executors.newSingleThreadExecutor(
      Executors.defaultThreadFactory());

  /** Runs a C++ function on the main thread using the executor. */
  public static void runOnMainThread(Activity activity, final CppThreadDispatcherContext context) {
    Object unused = executor.submit(new Runnable() {
        @Override
        public void run() {
          context.execute();
        }
      });
  }

  /** Runs a C++ function on a new Java background thread. */
  public static void runOnBackgroundThread(final CppThreadDispatcherContext context) {
    Thread t = new Thread(new Runnable() {
      @Override
      public void run() {
        context.execute();
      }
    });
    t.start();
  }
}
