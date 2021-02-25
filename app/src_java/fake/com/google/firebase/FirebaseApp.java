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
import java.util.HashMap;

/** Fake //j/c/g/a/gmscore/integ/client/firebase_common/src/com/google/firebase/FirebaseApp.java */
public final class FirebaseApp {
  static final String DEFAULT_NAME = "[DEFAULT]";
  static final HashMap<String, FirebaseApp> instances = new HashMap<>();

  String name;
  FirebaseOptions options;

  // Exposed to clear all FirebaseApp instances.  This should be called between each test case.
  public static void reset() {
    instances.clear();
  }

  public static FirebaseApp initializeApp(Context context, FirebaseOptions options) {
    return initializeApp(context, options, DEFAULT_NAME);
  }

  public static FirebaseApp initializeApp(Context context, FirebaseOptions options, String name) {
    if (!instances.containsKey(name)) {
      instances.put(name, new FirebaseApp(name, options));
    }
    return getInstance(name);
  }

  public static FirebaseApp getInstance() {
    return getInstance(DEFAULT_NAME);
  }

  public static FirebaseApp getInstance(String name) {
    FirebaseApp app = instances.get(name);
    if (app == null) {
      throw new IllegalStateException(String.format("FirebaseApp %s does not exist", name));
    }
    return app;
  }

  private FirebaseApp(String name, FirebaseOptions options) {
    this.name = name;
    this.options = options;
  }

  public void delete() {
    instances.remove(name);
  }

  public FirebaseOptions getOptions() {
    return options;
  }

  public boolean isDataCollectionDefaultEnabled() {
    return true;
  }

  public void setDataCollectionDefaultEnabled(boolean enabled) {}
}
