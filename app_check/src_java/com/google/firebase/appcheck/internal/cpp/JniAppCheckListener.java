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

import androidx.annotation.NonNull;
import com.google.firebase.appcheck.AppCheckToken;
import com.google.firebase.appcheck.FirebaseAppCheck.AppCheckListener;

/**
 * An AppCheckListener that notifies C++ of token changes.
 */
public class JniAppCheckListener implements AppCheckListener {
  // A C++ pointer to AppCheckInternal
  private long cAppCheck;

  JniAppCheckListener(long cAppCheck) {
    this.cAppCheck = cAppCheck;
  }

  public void onAppCheckTokenChanged(@NonNull AppCheckToken token) {
    nativeOnAppCheckTokenChanged(cAppCheck, token);
  }

  /**
   * This function is implemented in the AppCheck C++ library (app_check_android.cc).
   */
  private native void nativeOnAppCheckTokenChanged(long cAppCheck, AppCheckToken token);
}
