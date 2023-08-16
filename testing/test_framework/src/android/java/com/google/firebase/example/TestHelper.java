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

import android.content.ContentResolver;
import android.content.Context;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

import java.lang.Class;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.math.BigInteger;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Collection;

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
    // Use reflection to return GoogleApiAvailability.getInstance().getApkVersion(context);
    // This avoids needing Google Play services to be present (and returns 0 if it's not).
    try {
      // GoogleApiAvailability
      Class<?> googleApiAvailabilityClass =
          Class.forName("com.google.android.gms.common.GoogleApiAvailability");

      // .getInstance()
      Method getInstanceMethod = googleApiAvailabilityClass.getDeclaredMethod("getInstance");
      Object instance = getInstanceMethod.invoke(null);

      // .getApkVersion(context)
      Class[] getApkVersionParams = new Class[] {Class.forName("android.content.Context")};
      Method getApkVersionMethod =
          googleApiAvailabilityClass.getMethod("getApkVersion", getApkVersionParams);
      Object apkVersionObject = getApkVersionMethod.invoke(instance, context);
      if (apkVersionObject != null && apkVersionObject instanceof Integer) {
        return ((Integer) apkVersionObject).intValue();
      }
    } catch (Exception e) {
      Log.e(TAG, e.toString());
    }
    return 0;
  }

  private static String md5(String string) {
    // Old devices have a bug where OpenSSL can leave MessageDigest in a bad state, but trying
    // multiple times seems to clear it.
    int maxAttempts = 3;
    for (int i = 0; i < maxAttempts; ++i) {
      try {
        MessageDigest md5 = MessageDigest.getInstance("MD5");
        md5.update(string.getBytes());
        return String.format("%032X", new BigInteger(1, md5.digest()));
      } catch (NoSuchAlgorithmException e) {
        // Try again.
      } catch (ArithmeticException ex) {
        return "";
      }
    }
    return "";
  }

  public static String getDebugDeviceId(Context context) {
      if (isRunningOnEmulator()) {
	  return "emulator";
      }
      ContentResolver contentResolver = context.getContentResolver();
      String androidId =
          contentResolver == null
	  ? "null"
	  : Settings.Secure.getString(contentResolver, Settings.Secure.ANDROID_ID);
      return androidId;
  }
}
