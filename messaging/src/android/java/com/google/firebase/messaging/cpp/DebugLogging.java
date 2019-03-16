// Copyright 2016 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.firebase.messaging.cpp;

import android.util.Log;

/**
 * This is here to make a single place we can strip logging, with the enableLogging flag
 * and additionaly is a convenient place to re-route java logging to use the firebase
 * logging if/when we expose it to java
 */
public class DebugLogging {
  private static final boolean ENABLE_LOGGING = false;

  public static final void log(String tag, String msg) {
    if (ENABLE_LOGGING) {
      Log.d(tag, msg);
    }
  }

  public static final void log(String tag, String msg, Throwable tr) {
    if (ENABLE_LOGGING) {
      Log.d(tag, msg, tr);
    }
  }
}
