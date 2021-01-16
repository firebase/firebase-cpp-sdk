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

  private static final String FN_GET_FETCH_TIMEOUT =
      "FirebaseRemoteConfig.getFetchTimeoutInSeconds";
  private static final String FN_GET_MIN_FETCH_INTERVAL =
      "FirebaseRemoteConfig.getMinimumFetchIntervalInSeconds";
  private static final String FN_SET_FETCH_TIMEOUT =
      "FirebaseRemoteConfigSettings.Builder.setFetchTimeoutInSeconds";
  private static final String FN_SET_MIN_FETCH_INTERVAL =
      "FirebaseRemoteConfigSettings.Builder.setMinimumFetchIntervalInSeconds";

  public long getFetchTimeoutInSeconds() {
    ConfigRow row = ConfigAndroid.get(FN_GET_FETCH_TIMEOUT);
    long result = row.returnvalue().tlong();
    FakeReporter.addReportWithResult(FN_GET_FETCH_TIMEOUT, String.valueOf(result));
    return result;
  }

  public long getMinimumFetchIntervalInSeconds() {
    ConfigRow row = ConfigAndroid.get(FN_GET_MIN_FETCH_INTERVAL);
    long result = row.returnvalue().tlong();
    FakeReporter.addReportWithResult(FN_GET_MIN_FETCH_INTERVAL, String.valueOf(result));
    return result;
  }

  /** Fake Builder */
  public static class Builder {
    public Builder setFetchTimeoutInSeconds(long fetchTimeout) {
      FakeReporter.addReport(FN_SET_FETCH_TIMEOUT, String.valueOf(fetchTimeout));
      return this;
    }

    public Builder setMinimumFetchIntervalInSeconds(long minFetchInterval) {
      FakeReporter.addReport(FN_SET_MIN_FETCH_INTERVAL, String.valueOf(minFetchInterval));
      return this;
    }

    public FirebaseRemoteConfigSettings build() {
      FakeReporter.addReport("FirebaseRemoteConfigSettings.Builder.build");
      return new FirebaseRemoteConfigSettings();
    }
  }
}
