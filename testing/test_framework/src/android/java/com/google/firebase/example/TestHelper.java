// Copyright 2019 Google Inc. All rights reserved.
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

import android.content.Context;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.text.format.Formatter;
import android.util.Log;

/**
 * A simple class with test helper methods.
 */

public final class TestHelper {
  private static final String TAG = "TestHelper";
  private static String getDeviceIpAddress(Context context) {
    WifiManager wm =
        (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
    String ip = Formatter.formatIpAddress(wm.getConnectionInfo().getIpAddress());

    // For diagnosis, you may want this temporarily to be able to check the TestLab device logcat
    // logs
    Log.i(TAG, "isTestLabVirtualIpAddress: ip: " + ip);
    return ip;
  }

  public static boolean isRunningOnEmulator() {
    return Build.BRAND.contains("generic") || Build.DEVICE.contains("generic")
        || Build.PRODUCT.contains("sdk") || Build.HARDWARE.contains("goldfish")
        || Build.MANUFACTURER.contains("Genymotion") || Build.PRODUCT.contains("vbox86p")
        || Build.DEVICE.contains("vbox86p") || Build.HARDWARE.contains("vbox86");
  }
}
