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

package com.google.firebase.remoteconfig;

import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.firebase.testing.cppsdk.ConfigAndroid;
import com.google.firebase.testing.cppsdk.ConfigRow;
import com.google.firebase.testing.cppsdk.FakeReporter;

/** Fake FirebaseRemoteConfigValue */
public class FirebaseRemoteConfigValue {

  public FirebaseRemoteConfigValue() {}

  public long asLong() {
    ConfigRow row = ConfigAndroid.get("FirebaseRemoteConfigValue.asLong");
    long result = row.returnvalue().tlong();
    FakeReporter.addReportWithResult("FirebaseRemoteConfigValue.asLong", Long.toString(result));
    return result;
  }

  public double asDouble() {
    ConfigRow row = ConfigAndroid.get("FirebaseRemoteConfigValue.asDouble");
    double result = row.returnvalue().tdouble();
    FakeReporter.addReportWithResult(
        "FirebaseRemoteConfigValue.asDouble", String.format("%.3f", result));
    return result;
  }

  public String asString() {
    ConfigRow row = ConfigAndroid.get("FirebaseRemoteConfigValue.asString");
    String result = row.returnvalue().tstring();
    FakeReporter.addReportWithResult("FirebaseRemoteConfigValue.asString", result);
    return result;
  }

  public byte[] asByteArray() {
    ConfigRow row = ConfigAndroid.get("FirebaseRemoteConfigValue.asByteArray");
    byte[] result = {};
    result = row.returnvalue().tstring().getBytes(UTF_8);
    FakeReporter.addReportWithResult("FirebaseRemoteConfigValue.asByteArray", new String(result));
    return result;
  }

  public boolean asBoolean() {
    ConfigRow row = ConfigAndroid.get("FirebaseRemoteConfigValue.asBoolean");
    boolean result = row.returnvalue().tbool();
    FakeReporter.addReportWithResult("FirebaseRemoteConfigValue.asBoolean", String.valueOf(result));
    return result;
  };

  public int getSource() {
    ConfigRow row = ConfigAndroid.get("FirebaseRemoteConfigValue.getSource");
    int result = row.returnvalue().tint();
    FakeReporter.addReportWithResult(
        "FirebaseRemoteConfigValue.getSource", Integer.toString(result));
    return result;
  }
}
