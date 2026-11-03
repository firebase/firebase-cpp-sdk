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

/** Mock InstallationTokenResult */
public class InstallationTokenResult {
  private final String token;
  private final long expirationTimestamp;
  private final long creationTimestamp;

  private InstallationTokenResult(String token) {
    this.token = token;
    this.expirationTimestamp = 3600000L;
    this.creationTimestamp = 0L;
  }

  public static synchronized InstallationTokenResult getInstance() {
    return new InstallationTokenResult("FakeToken");
  }

  public static synchronized InstallationTokenResult getInstance(boolean forceRefresh) {
    return new InstallationTokenResult(forceRefresh ? "FakeTokenForceRefresh" : "FakeToken");
  }

  public String getToken() {
    return token;
  }

  public long getTokenExpirationTimestamp() {
    return expirationTimestamp;
  }

  public long getTokenCreationTimestamp() {
    return creationTimestamp;
  }
}
