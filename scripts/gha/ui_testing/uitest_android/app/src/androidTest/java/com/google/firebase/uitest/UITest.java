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

package com.google.firebase.uitest;

import static androidx.test.core.app.ApplicationProvider.getApplicationContext;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;
import androidx.core.content.FileProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObject2;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;
import androidx.test.uiautomator.Until;
import java.io.File;
import java.io.IOException;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

/** UI Test class, launch testapps and perform UI functions. */
@RunWith(AndroidJUnit4.class)
public class UITest {
  private static final String TAG = "UITestResult";

  private static final String MESSAGING_PACKAGE = "com.google.firebase.cpp.messaging.testapp";

  private static final int DEFAULT_TIMEOUT = 5000;

  private static final int WAIT_UI_TIMEOUT = 30000;

  private void launchApp(String packageName) {
    Context context = getApplicationContext();
    Intent intent = context.getPackageManager().getLaunchIntentForPackage(packageName);
    intent.setAction("com.google.intent.action.UI_TEST");
    intent.addCategory(Intent.CATEGORY_DEFAULT);
    intent.setType("application/javascript");
    intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK); // Clear out any previous instances

    String gamePackageName = "com.google.android.admob.testapp";
    File dir = new File(context.getFilesDir(), gamePackageName);
    if (!dir.exists())
      dir.mkdirs();
    String filename = "Results1.json";
    File file = new File(dir, filename);
    try {
      file.createNewFile();
    } catch (IOException e) {
      e.printStackTrace();
    }
    Log.d("TAG", "Test Result Path :" + file);
    Uri fileUri =
        FileProvider.getUriForFile(context, "com.google.firebase.uitest.fileprovider", file);
    intent.setPackage(gamePackageName)
        .setDataAndType(fileUri, "application/javascript")
        .addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
    context.startActivity(intent);
  }
}
