// Copyright 2019 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.firebase.example;

import android.app.Activity;
import android.content.SharedPreferences;

/** Static utilties for saving and loading shared preference strings. */
public final class SimplePersistentStorage {
  private static final String PREF_NAME = "firebase_automated_test";
  /**
   * Sets a given key's value in persistent storage to the given string. Specify null to delete the
   * key.
   */
  public static void setString(Activity activity, String key, String value) {
    SharedPreferences pref = activity.getSharedPreferences(PREF_NAME, 0);
    SharedPreferences.Editor editor = pref.edit();
    if (value != null) {
      editor.putString(key, value);
    } else {
      editor.remove(key);
    }
    editor.commit();
  }

  /** Gets the value of the given key in persistent storage, or null if the key is not found. */
  public static String getString(Activity activity, String key) {
    SharedPreferences pref = activity.getSharedPreferences(PREF_NAME, 0);
    return pref.getString(key, null);
  }

  private SimplePersistentStorage() {}
}
