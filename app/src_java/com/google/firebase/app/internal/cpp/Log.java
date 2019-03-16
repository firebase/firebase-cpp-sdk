/*
 * Copyright 2016 Google LLC
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

/**
 * Replacement for android.util.Log that redirects log messages to the
 * C++ log methods in log_android.cc.
 */
public class Log {
  private static final String TAG = "firebase_log";

  // Global log instance.
  private static Log sLogInstance = null;
  // Lock which is used to arbitrate access to the global logger.
  private static final Object sLock = new Object();

  // Flag which is set to false if we detect the nativeLog method has been unregistered.
  private boolean nativeLogAvailable = true;

  /**
   * See android.util.Log.d().
   */
  public static int d(String tag, String msg) {
    safeNativeLog(android.util.Log.DEBUG, tag, msg);
    return 0;
  }

  /**
   * See android.util.Log.v().
   */
  public static int v(String tag, String msg) {
    safeNativeLog(android.util.Log.VERBOSE, tag, msg);
    return 0;
  }

  /**
   * See android.util.Log.i().
   */
  public static int i(String tag, String msg) {
    safeNativeLog(android.util.Log.INFO, tag, msg);
    return 0;
  }

  /**
   * See android.util.Log.w().
   */
  public static int w(String tag, String msg) {
    safeNativeLog(android.util.Log.WARN, tag, msg);
    return 0;
  }

  /**
   * See android.util.Log.e().
   */
  public static int e(String tag, String msg) {
    safeNativeLog(android.util.Log.ERROR, tag, msg);
    return 0;
  }

  /**
   * See android.util.Log.wtf().
   */
  public static int wtf(String tag, String msg) {

    safeNativeLog(android.util.Log.ERROR, tag, msg);
    return 0;
  }

  /**
   * Get / create the Log singleton.
   */
  public static Log getInstance() {
    synchronized (sLock) {
      if (sLogInstance == null) {
        android.util.Log.d(TAG, "Creating Log instance.");
        sLogInstance = new Log();
        sLogInstance.safeNativeLogInternal(
            android.util.Log.DEBUG, TAG, sLogInstance.getClass().toString());
      }
    }
    return sLogInstance;
  }

  /**
   * Dereference the log singleton.
   */
  public static void shutdown() {
    synchronized (sLock) {
      sLogInstance = null;
    }
  }

  /**
   * Static wrapper around safeNativeLogInternal which gets / creates an instance of the log
   * class then calls safeNativeLogInternal to ideally log via nativeLog or fall back to
   * android.util.Log.
   */
  private static void safeNativeLog(int level, String tag, String msg) {
    synchronized (sLock) {
      getInstance().safeNativeLogInternal(level, tag, msg);
    }
  }

  /**
   * Wrapper for nativeLog that will redirect log messages to android.util.Log if a native method
   * isn't registered for the Log.nativeLog method.
   */
  private void safeNativeLogInternal(int level, String tag, String msg) {
    if (nativeLogAvailable) {
      try {
        nativeLog(level, tag, msg);
      } catch (UnsatisfiedLinkError e) {
        nativeLogAvailable = false;
        android.util.Log.w(
            TAG, String.format("nativeLog not registered, falling back to android.util.Log (%s)",
                e.toString()));
      }
    }
    if (!nativeLogAvailable) {
      switch (level) {
        case android.util.Log.VERBOSE:
          android.util.Log.v(tag, msg);
          break;
        case android.util.Log.INFO:
          android.util.Log.i(tag, msg);
          break;
        case android.util.Log.WARN:
          android.util.Log.w(tag, msg);
          break;
        case android.util.Log.ERROR:
          android.util.Log.e(tag, msg);
          break;
        case android.util.Log.DEBUG:
        default:
          android.util.Log.d(tag, msg);
          break;
      }
    }
  }

  private native void nativeLog(int level, String tag, String msg);
}
