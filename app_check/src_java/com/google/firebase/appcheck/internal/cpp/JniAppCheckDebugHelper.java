/*
 * Copyright 2023 Google LLC
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

package com.google.firebase.appcheck.internal.cpp;

import android.content.Context;
import android.content.SharedPreferences;
import com.google.firebase.FirebaseApp;

/**
 * Java logic to write the debug token to the same location that the Android
 * SDK will read from. This allows users to set the Debug token from code,
 * instead of trying to use environment variables and helper classes.
 */
public class JniAppCheckDebugHelper {
  static final String PREFS_TEMPLATE = "com.google.firebase.appcheck.debug.store.%s";

  static final String DEBUG_SECRET_KEY = "com.google.firebase.appcheck.debug.DEBUG_SECRET";

  public static void SetDebugToken(FirebaseApp firebaseApp, String token) {
    Context context = firebaseApp.getApplicationContext();
    String prefsName = String.format(PREFS_TEMPLATE, firebaseApp.getPersistenceKey());
    SharedPreferences sharedPreferences =
        context.getSharedPreferences(prefsName, Context.MODE_PRIVATE);
    sharedPreferences.edit().putString(DEBUG_SECRET_KEY, token).apply();
  }
}
