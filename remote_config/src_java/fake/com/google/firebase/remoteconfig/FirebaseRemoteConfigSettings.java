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

import com.google.firebase.testing.cppsdk.ConfigAndroid;
import com.google.firebase.testing.cppsdk.ConfigRow;
import com.google.firebase.testing.cppsdk.FakeReporter;

/** Fake FirebaseRemoteConfigSettings */
public class FirebaseRemoteConfigSettings {

  public boolean isDeveloperModeEnabled() {
    ConfigRow row = ConfigAndroid.get("FirebaseRemoteConfigSettings.isDeveloperModeEnabled");
    boolean result = row.returnvalue().tbool();
    FakeReporter.addReportWithResult(
        "FirebaseRemoteConfigSettings.isDeveloperModeEnabled", String.valueOf(result));
    return result;
  }

  /** Fake Builder */
  public static class Builder {
    public Builder setDeveloperModeEnabled(boolean enabled) {
      FakeReporter.addReport(
          "FirebaseRemoteConfigSettings.Builder.setDeveloperModeEnabled", String.valueOf(enabled));
      return this;
    }

    public FirebaseRemoteConfigSettings build() {
      FakeReporter.addReport("FirebaseRemoteConfigSettings.Builder.build");
      return new FirebaseRemoteConfigSettings();
    }
  }
}
