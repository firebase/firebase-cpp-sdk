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
import com.google.firebase.testing.cppsdk.ConfigAndroid;
import com.google.firebase.testing.cppsdk.ConfigRow;
import com.google.firebase.testing.cppsdk.FutureBoolResult;
import com.google.firebase.testing.cppsdk.TickerAndroid;
import com.google.firebase.testing.cppsdk.TickerObserver;

/**
 * Fake //f/a/c/cpp/src_java/com/google/firebase/app/internal/cpp/GoogleApiAvailabilityHelper.java
 */
public final class GoogleApiAvailabilityHelper {
  private static final int SUCCESS = 0;

  public static boolean makeGooglePlayServicesAvailable(Activity activity) {
    final ConfigRow row =
        ConfigAndroid.get("GoogleApiAvailabilityHelper.makeGooglePlayServicesAvailable");
    if (row != null) {
      TickerAndroid.register(
          new TickerObserver() {
            @Override
            public void elapse() {
              if (TickerAndroid.now() == row.futureint().ticker()) {
                int resultCode = row.futureint().value();
                onCompleteNative(resultCode, "result code is " + resultCode);
              }
            }
          });
      return row.futurebool().value() == FutureBoolResult.True;
    }

    // Default behavior
    onCompleteNative(SUCCESS, "Google Play services are already available (fake)");
    return true;
  }

  public static void stopCallbacks() {}

  private static native void onCompleteNative(int resultCode, String resultMessage);
}
