/*
 * Copyright 2021 Google LLC
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

package com.google.firebase.gma.internal.cpp;

import android.content.Context;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.initialization.InitializationStatus;
import com.google.android.gms.ads.initialization.OnInitializationCompleteListener;

/** Helper class for initializing the Google Mobile Ads SDK. */
public final class GmaInitializationHelper {
  public static void initializeGma(Context context) {
    MobileAds.initialize(context, new OnInitializationCompleteListener() {
      @Override
      public void onInitializationComplete(InitializationStatus initializationStatus) {
        initializationCompleteCallback(initializationStatus);
      }
    });
  }

  public static native void initializationCompleteCallback(
      InitializationStatus initializationStatus);
}
