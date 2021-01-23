// Copyright 2020 Google LLC
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

package com.google.firebase.installations;

import com.google.android.gms.tasks.Task;
import com.google.firebase.FirebaseApp;
import com.google.firebase.testing.cppsdk.FakeReporter;
import com.google.firebase.testing.cppsdk.TickerAndroid;

/** Mock FirebaseInstallations. */
public class FirebaseInstallations {

  private static final String FN_GET_ID = "FirebaseInstallations.getId";
  private static final String FN_GEI_TOKEN = "FirebaseInstallations.getToken";
  private static final String FN_DELETE = "FirebaseInstallations.delete";

  private FirebaseInstallations() {
    FakeReporter.addReport("FirebaseInstallations.construct");
  }

  public static synchronized FirebaseInstallations getInstance(FirebaseApp app) {
    return new FirebaseInstallations();
  }

  public Task<String> getId() {
    FakeReporter.addReport("FirebaseInstallations.getId");
    return stringHelper(FN_GET_ID);
  }

  public Task<InstallationTokenResult> getToken(boolean forceRefresh) {
    FakeReporter.addReport("FirebaseInstallations.getToken");
    return tokenHelper(FN_GEI_TOKEN);
  }

  public Task<Void> delete() {
    FakeReporter.addReport("FirebaseInstallations.delete");
    return voidHelper(FN_DELETE);
  }

  private static Task<Void> voidHelper(String configKey) {
    Task<Void> result = Task.forResult(configKey, null);
    TickerAndroid.register(result);
    return result;
  }

  private static Task<String> stringHelper(String configKey) {
    Task<String> result = Task.forResult(configKey, "FakeId");
    TickerAndroid.register(result);
    return result;
  }

  private static Task<InstallationTokenResult> tokenHelper(String configKey) {
    Task<InstallationTokenResult> result =
        Task.forResult(configKey, InstallationTokenResult.getInstance());
    TickerAndroid.register(result);
    return result;
  }
}
