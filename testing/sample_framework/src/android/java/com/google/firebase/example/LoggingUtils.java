// Copyright 2016 Google Inc. All rights reserved.
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

import android.app.Activity;
import android.content.Intent;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.DataOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * A utility class, encapsulating the data and methods required to log arbitrary
 * text to the screen, via a non-editable TextView.
 */
public class LoggingUtils {
  private static TextView textView = null;
  private static ScrollView scrollView = null;
  // Tracks if the log window was touched at least once since the testapp was started.
  private static boolean didTouch = false;
  // If a test log file is specified, this is the log file's URI...
  private static Uri logFile = null;
  private static boolean shouldRunUITests = true;
  private static boolean shouldRunNonUITests = true;
  // ...and this is the stream to write to.
  private static DataOutputStream logFileStream = null;

  /** Initializes the log window with the given activity and a monospace font. */
  public static void initLogWindow(Activity activity) {
    initLogWindow(activity, true);
  }

  /**
   * Initializes the log window with the given activity, specifying whether to use a monospaced
   * font.
   */
  public static void initLogWindow(Activity activity, boolean monospace) {
    LinearLayout linearLayout = new LinearLayout(activity);
    scrollView = new ScrollView(activity);
    textView = new TextView(activity);
    textView.setTag("Logger");
    if (monospace) {
      textView.setTypeface(Typeface.MONOSPACE);
      textView.setTextSize(10);
    }
    linearLayout.addView(scrollView);
    scrollView.addView(textView);
    Window window = activity.getWindow();
    window.takeSurface(null);
    window.setContentView(linearLayout);

    // Force the TextView to stay scrolled to the bottom.
    textView.addTextChangedListener(new TextWatcher() {
      @Override
      public void afterTextChanged(Editable e) {
        // If the user never interacted with the screen, scroll to bottom.
        if (scrollView != null && !didTouch) {
          new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
              scrollView.fullScroll(View.FOCUS_DOWN);
            }
          });
        }
      }

      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

      @Override
      public void onTextChanged(CharSequence s, int start, int count, int after) {}
    });
    textView.setOnTouchListener(new View.OnTouchListener() {
      @Override
      public boolean onTouch(View v, MotionEvent event) {
        didTouch = true;
        return false;
      }
    });

    Intent launchIntent = activity.getIntent();
    // Check if we are running on Firebase Test Lab, and set up a log file if we are.
    if (launchIntent.getAction().equals("com.google.intent.action.TEST_LOOP")) {
      shouldRunUITests = false;
      startLogFile(activity, launchIntent.getData().toString());
    } else if (launchIntent.getAction().equals("com.google.intent.action.UI_TEST")) {
      shouldRunNonUITests = false;
      startLogFile(activity, launchIntent.getData().toString());
    }
  }

  /** Adds some text to the log window. */
  public static void addLogText(final String text) {
    new Handler(Looper.getMainLooper()).post(new Runnable() {
      @Override
      public void run() {
        if (textView != null) {
          textView.append(text);
          if (logFileStream != null) {
            try {
              logFileStream.writeBytes(text);
            } catch (IOException e) {
              // It doesn't really matter if something went wrong writing to the test log.
            }
          }
        }
      }
    });
  }

  /**
   * Returns true if the user ever touched the log window during this run (to scroll it or
   * otherwise), false if they never have.
   */
  public static boolean getDidTouch() {
    return didTouch;
  }

  /** Start logging to a file at the given URI string. Used in TEST_LOOP mode. */
  public static boolean startLogFile(Activity activity, String logFileUri) {
    logFile = Uri.parse(logFileUri);
    if (logFile != null) {
      try {
        logFileStream =
            new DataOutputStream(activity.getContentResolver().openOutputStream(logFile));
      } catch (FileNotFoundException e) {
        addLogText("Failed to open log file " + logFile.getEncodedPath() + ": " + e);
        return false;
      }
      return true;
    }
    return false;
  }

  /**
   * If the logger is logging to a file in local storage, return the URI for that file (which you
   * can later pass into startLogFile in the future), otherwise return null if not logging to file.
   */
  public static String getLogFile() {
    if (logFile != null && logFileStream != null) {
      return logFile.toString();
    } else {
      return null;
    }
  }

  public static boolean shouldRunUITests() {
    return shouldRunUITests;
  }

  public static boolean shouldRunNonUITests() {
    return shouldRunNonUITests;
  }
}
