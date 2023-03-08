// Copyright 2022 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.firebase.example;

import android.content.Context;
import android.os.Build;
import com.google.android.gms.common.GoogleApiAvailability;

/**
 * A simple class with test helper methods.
 */

public final class TestHelper {
  private static final String TAG = "TestHelper";
  public static boolean isRunningOnEmulator() {
    return Build.BRAND.contains("generic") || Build.DEVICE.contains("generic")
        || Build.PRODUCT.contains("sdk") || Build.HARDWARE.contains("goldfish")
        || Build.MANUFACTURER.contains("Genymotion") || Build.PRODUCT.contains("vbox86p")
        || Build.DEVICE.contains("vbox86p") || Build.HARDWARE.contains("vbox86");
  }
  public static int getGooglePlayServicesVersion(Context context) {
    return GoogleApiAvailability.getInstance().getApkVersion(context);
  }
}
