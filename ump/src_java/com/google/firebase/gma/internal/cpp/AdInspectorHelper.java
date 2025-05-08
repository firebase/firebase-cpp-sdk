/*
 * Copyright 2022 Google LLC
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

import android.util.Log;
import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdInspectorError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.OnAdInspectorClosedListener;

/** Helper class for listening to AdInspector closed events. */
public final class AdInspectorHelper implements OnAdInspectorClosedListener {
  /**
   * Pointer to the C++ AdInspectorClosedListener object to invoke when the AdInspector has been
   * closed.
   */
  private long mNativeCallbackPtr;

  /** Constructor. */
  AdInspectorHelper(long nativeCallbackPtr) {
    mNativeCallbackPtr = nativeCallbackPtr;
  }

  /** Method that the Android GMA SDK invokes when the AdInspector has been closed. */
  @Override
  public void onAdInspectorClosed(AdInspectorError error) {
    adInspectorClosedCallback(mNativeCallbackPtr, error);
  }

  /**
   * Native callback to which will signal the customer's application that the AdInspector has been
   * closed. A null AdError signifies success.
   */
  public static native void adInspectorClosedCallback(long nativeCallbackPtr, AdError error);
}
