// Copyright 2017 Google LLC
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

package com.google.firebase.messaging;

import android.text.TextUtils;
import com.google.android.gms.tasks.Task;
import com.google.firebase.testing.cppsdk.FakeReporter;
import com.google.firebase.testing.cppsdk.TickerAndroid;
import java.util.ArrayList;

/**
 * Fake FirebaseMessaging class.
 */
public class FirebaseMessaging {

  private static final String FN_GET_TOKEN = "FirebaseMessaging.getToken";
  private static final String FN_DELETE_TOKEN = "FirebaseMessaging.deleteToken";

  public static synchronized FirebaseMessaging getInstance() {
    return new FirebaseMessaging();
  }

  public boolean isAutoInitEnabled() {
    return autoInitEnabled_;
  }

  public void setAutoInitEnabled(boolean enable) {
    autoInitEnabled_ = enable;
  }

  public void send(RemoteMessage message) {
    FakeReporter.addReport("FirebaseMessaging.send", String.valueOf(message.to),
        String.valueOf(message.data), String.valueOf(message.messageId),
        String.valueOf(message.from), String.valueOf(message.ttl));
    if (TextUtils.isEmpty(message.to)) {
      throw new IllegalArgumentException("Missing 'to'");
    }
  }

  public Task<Void> subscribeToTopic(String topic) {
    String fake = "FirebaseMessaging.subscribeToTopic";
    ArrayList<String> args = new ArrayList<>(FakeReporter.getFakeArgs(fake));
    args.add(String.valueOf(topic));
    FakeReporter.addReport(fake, args.toArray(new String[0]));
    if (TextUtils.isEmpty(topic) || "$invalid".equals(topic)) {
      throw new IllegalArgumentException("Invalid topic: " + topic);
    }
    return Task.forResult(fake, null);
  }

  public Task<Void> unsubscribeFromTopic(String topic) {
    String fake = "FirebaseMessaging.unsubscribeFromTopic";
    ArrayList<String> args = new ArrayList<>(FakeReporter.getFakeArgs(fake));
    args.add(String.valueOf(topic));
    FakeReporter.addReport(fake, args.toArray(new String[0]));
    if (TextUtils.isEmpty(topic) || "$invalid".equals(topic)) {
      throw new IllegalArgumentException("Invalid topic: " + topic);
    }
    return Task.forResult(fake, null);
  }

  public void setDeliveryMetricsExportToBigQuery(boolean enable) {
    deliveryMetricsExportToBigQueryEnabled = enable;
  }

  public boolean deliveryMetricsExportToBigQueryEnabled() {
    return deliveryMetricsExportToBigQueryEnabled;
  }

  public Task<String> getToken() {
    FakeReporter.addReport(FN_GET_TOKEN);
    Task<String> result = Task.forResult(FN_GET_TOKEN, "StubToken");
    TickerAndroid.register(result);
    return result;
  }

  public Task<Void> deleteToken() {
    FakeReporter.addReport(FN_DELETE_TOKEN);
    Task<Void> result = Task.forResult(FN_DELETE_TOKEN, null);
    TickerAndroid.register(result);
    return result;
  }

  private boolean deliveryMetricsExportToBigQueryEnabled = false;

  private boolean autoInitEnabled_ = true;
}
