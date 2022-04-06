// Copyright 2020 Google Inc. All rights reserved.
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

package com.google.firebase.test;

import static androidx.test.core.app.ApplicationProvider.getApplicationContext;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.util.Log;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObject2;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;
import androidx.test.uiautomator.Until;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** UI Test class, launch testapps and perform UI functions. */
@RunWith(AndroidJUnit4.class)
public class UITest {

  private static final String TAG = "UITestResult";

  private static final String GMA_PACKAGE = "com.google.android.admob.testapp";

  private static final String MESSAGING_PACKAGE = "com.google.firebase.cpp.messaging.testapp";

  private static final int LAUNCH_TIMEOUT = 5000;


  @Test
  public void testGMA() throws UiObjectNotFoundException, InterruptedException {
    // Start from the home screen & Launch the app
    UiDevice device = UiDevice.getInstance(getInstrumentation());
    device.pressHome();
    launchApp(GMA_PACKAGE);
    device.wait(Until.hasObject(By.pkg(GMA_PACKAGE).depth(0)), LAUNCH_TIMEOUT); // Wait for the app to appear
    Log.e(TAG, "GMA launched");

    // TestAdViewAdClick
    UiObject2 reference = device.wait(Until.findObject(By.text("Test Ad")), 60 * 1000);
    Assert.assertNotNull(reference);
    Log.e(TAG, "TestAdVie loaded");
    Thread.sleep(5 * 1000);
    int x = reference.getVisibleBounds().centerX();
    int y = reference.getVisibleBounds().bottom + 5;
    device.click(x, y);
    Thread.sleep(5 * 1000);
    bringToForeground(GMA_PACKAGE);
    Log.e(TAG, "TestAdViewAdClick closed");

    Thread.sleep(5 * 1000);

    // TestInterstitialAdClose
    reference = device.wait(Until.findObject(By.text("Test Ad")), 10 * 1000);
    Assert.assertNotNull(reference);
    Log.e(TAG, "InterstitialAd loaded");
    Thread.sleep(5 * 1000);
    x = reference.getVisibleBounds().bottom;
    y = reference.getVisibleBounds().bottom;
    device.click(x, y);
    Log.e(TAG, "InterstitialAd closed");

    Thread.sleep(5 * 1000);

    // TestInterstitialAdClickAndClose
    reference = device.wait(Until.findObject(By.text("Test Ad")), 10 * 1000);
    Assert.assertNotNull(reference);
    Log.e(TAG, "InterstitialAd2 loaded");
    Thread.sleep(5 * 1000);
    x = device.getDisplayWidth()/2;
    y = device.getDisplayHeight()/2;
    device.click(x, y);
    Log.e(TAG, "InterstitialAd2 clicked");
    Thread.sleep(5 * 1000);
    bringToForeground(GMA_PACKAGE);
    Thread.sleep(5 * 1000);
    x = reference.getVisibleBounds().bottom;
    y = reference.getVisibleBounds().bottom;
    device.click(x, y);
    Log.e(TAG, "InterstitialAd2 closed");

    Thread.sleep(5 * 1000);

    // TestRewardedAdClose
    UiObject countDown = device.findObject(new UiSelector().textContains("seconds"));
    Assert.assertTrue(countDown.waitForExists(10 * 1000));
    Log.e(TAG, "RewardedAd loaded");
    x = (countDown.getBounds().right + device.getDisplayWidth())/2;
    y = countDown.getBounds().centerY();
    Thread.sleep(10 * 1000);
    device.click(x, y);
    Log.e(TAG, "RewardedAd closed");

    // Finish GMA UI Tests
    Thread.sleep(10 * 1000);
    reference = device.wait(Until.findObject(By.text("Test Ad")), 5 * 1000);
    Assert.assertNull(reference);
  }

  // @Test
  // public void testMessaging() throws UiObjectNotFoundException {
  //   // Start from the home screen
  //   UiDevice mDevice = UiDevice.getInstance(getInstrumentation());
  //   mDevice.pressHome();
  //
  //   // Launch the app
  //   Context context = getApplicationContext();
  //   Intent intent = context.getPackageManager().getLaunchIntentForPackage(MESSAGING_PACKAGE);
  //   intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK); // Clear out any previous instances
  //   context.startActivity(intent);
  //
  //   // Wait for the app to appear
  //   mDevice.wait(Until.hasObject(By.pkg(MESSAGING_PACKAGE).depth(0)), LAUNCH_TIMEOUT);
  //
  //   // Wait for the notification sent
  //   boolean notificationSent =
  //       mDevice.wait(Until.hasObject(By.textContains("Notification sent.")), 3 * 60 * 1000);
  //   Assert.assertTrue(notificationSent);
  //
  //   // Click the notification
  //   mDevice.openNotification();
  //   UiObject2 notification =
  //       mDevice.wait(
  //           Until.findObject(By.text("Test notification, open to resume testing.")), 3 * 60 * 1000);
  //   Assert.assertNotNull(notification);
  //   notification.click();
  //
  //   // finished
  //   UiObject result = mDevice.findObject(new UiSelector().textContains("PASSED"));
  //   Assert.assertTrue(result.waitForExists(3 * 60 * 1000));
  //   Log.e(TAG, result.getText());
  // }

  private void launchApp(String packageName) {
    Context context = getApplicationContext();
    Intent intent = context.getPackageManager().getLaunchIntentForPackage(packageName);
    intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK); // Clear out any previous instances
    context.startActivity(intent);
  }

  private void bringToForeground(String packageName) {
    Context context = getApplicationContext();
    Intent intent = context.getPackageManager().getLaunchIntentForPackage(GMA_PACKAGE);
    intent.setFlags(Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED | Intent.FLAG_ACTIVITY_NEW_TASK);
    context.startActivity(intent);
  }
}
