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

package com.google.firebase.analytics;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import com.google.android.gms.tasks.Task;
import com.google.firebase.testing.cppsdk.FakeReporter;
import com.google.firebase.testing.cppsdk.TickerAndroid;

import java.util.TreeSet;

/**
 * Fake for FirebaseAnalytics.
 */
public final class FirebaseAnalytics {

  public static FirebaseAnalytics getInstance(Context context) {
    FakeReporter.addReport("FirebaseAnalytics.getInstance");
    return new FirebaseAnalytics();
  }

  public Task<String> getAppInstanceId() {
    FakeReporter.addReport("FirebaseAnalytics.getAppInstanceId");
    Task<String> result = Task.forResult("FakeAnalyticsInstanceId0");
    TickerAndroid.register(result);
    return result;
  }

  public void setAnalyticsCollectionEnabled(boolean enabled) {
    FakeReporter.addReport("FirebaseAnalytics.setAnalyticsCollectionEnabled",
        Boolean.toString(enabled));
  }

  public void logEvent(String name, Bundle params) {
    StringBuilder paramsString = new StringBuilder();
    // Sort keys for predictable ordering.
    for (String key : new TreeSet<>(params.keySet())) {
      paramsString.append(key);
      paramsString.append("=");
      paramsString.append(params.get(key));
      paramsString.append(",");
    }
    paramsString.setLength(Math.max(0, paramsString.length() - 1));
    FakeReporter.addReport("FirebaseAnalytics.logEvent", name, paramsString.toString());
  }

  public void resetAnalyticsData() {
    FakeReporter.addReport("FirebaseAnalytics.resetAnalyticsData");
  }

  public void setUserProperty(String name, String value) {
    FakeReporter.addReport("FirebaseAnalytics.setUserProperty", name, String.valueOf(value));
  }

  public void setCurrentScreen(Activity activity, String screenName,
      String screenClassOverride) {
    FakeReporter.addReport("FirebaseAnalytics.setCurrentScreen", activity.getClass().getName(),
        String.valueOf(screenName), String.valueOf(screenClassOverride));
  }

  public void setUserId(String userId) {
    FakeReporter.addReport("FirebaseAnalytics.setUserId", String.valueOf(userId));
  }

  public void setMinimumSessionDuration(long milliseconds) {
    FakeReporter.addReport("FirebaseAnalytics.setMinimumSessionDuration",
        Long.toString(milliseconds));
  }

  public void setSessionTimeoutDuration(long milliseconds) {
    FakeReporter.addReport("FirebaseAnalytics.setSessionTimeoutDuration",
        Long.toString(milliseconds));
  }

}
