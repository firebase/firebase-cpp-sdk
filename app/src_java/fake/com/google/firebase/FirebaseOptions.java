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

package com.google.firebase;

import android.content.Context;
import android.util.Log;

/**
 * Fake //j/c/g/a/gmscore/integ/client/firebase_common/src/com/google/firebase/FirebaseOptions.java
 */
public final class FirebaseOptions {
  private static final String LOG_TAG = "FakeFirebaseOptions";

  /** Fake Builder. */
  public static final class Builder {
    private String apiKey;
    private String applicationId;
    private String databaseUrl;
    private String gcmSenderId;
    private String storageBucket;
    private String projectId;

    public Builder setApiKey(String apiKey) {
      if (Log.isLoggable(LOG_TAG, Log.INFO)) {
        Log.i(LOG_TAG, "set api key " + apiKey);
      }
      this.apiKey = apiKey;
      return this;
    }

    public Builder setDatabaseUrl(String databaseUrl) {
      if (Log.isLoggable(LOG_TAG, Log.INFO)) {
        Log.i(LOG_TAG, "set database url " + databaseUrl);
      }
      this.databaseUrl = databaseUrl;
      return this;
    }

    public Builder setApplicationId(String applicationId) {
      if (Log.isLoggable(LOG_TAG, Log.INFO)) {
        Log.i(LOG_TAG, "set application id " + applicationId);
      }
      this.applicationId = applicationId;
      return this;
    }

    public Builder setGcmSenderId(String gcmSenderId) {
      if (Log.isLoggable(LOG_TAG, Log.INFO)) {
        Log.i(LOG_TAG, "set gcm sender id " + gcmSenderId);
      }
      this.gcmSenderId = gcmSenderId;
      return this;
    }

    public Builder setStorageBucket(String storageBucket) {
      if (Log.isLoggable(LOG_TAG, Log.INFO)) {
        Log.i(LOG_TAG, "set storage bucket " + storageBucket);
      }
      this.storageBucket = storageBucket;
      return this;
    }

    public Builder setProjectId(String projectId) {
      if (Log.isLoggable(LOG_TAG, Log.INFO)) {
        Log.i(LOG_TAG, "set project id " + projectId);
      }
      this.projectId = projectId;
      return this;
    }

    public FirebaseOptions build() {
      if (Log.isLoggable(LOG_TAG, Log.INFO)) {
        Log.i(LOG_TAG, "built");
      }
      return new FirebaseOptions(this);
    }
  }

  public Builder builder;

  private FirebaseOptions(Builder builder) {
    this.builder = builder;
  }

  public static FirebaseOptions fromResource(Context context) {
    return new FirebaseOptions(
        new Builder()
            .setApiKey("fake api key from resource")
            .setDatabaseUrl("fake database url from resource")
            .setApplicationId("fake app id from resource")
            .setGcmSenderId("fake messaging sender id from resource")
            .setStorageBucket("fake storage bucket from resource")
            .setProjectId("fake project id from resource"));
  }

  public String getApiKey() {
    return builder.apiKey;
  }

  public String getApplicationId() {
    return builder.applicationId;
  }

  public String getDatabaseUrl() {
    return builder.databaseUrl;
  }

  public String getGcmSenderId() {
    return builder.gcmSenderId;
  }

  public String getStorageBucket() {
    return builder.storageBucket;
  }

  public String getProjectId() {
    return builder.projectId;
  }
}
