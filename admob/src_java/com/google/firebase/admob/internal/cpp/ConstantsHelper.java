/*
 * Copyright 2016 Google LLC
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

package com.google.firebase.admob.internal.cpp;

/** Helper class containing constants that are shared across the various AdMob ad formats. */
public final class ConstantsHelper {

  /**
   * Error codes used in completing futures. These match the AdMobError enumeration in the C++ API.
   */
  // LINT.IfChange
  public static final int CALLBACK_ERROR_NONE = 0;

  public static final int CALLBACK_ERROR_UNINITIALIZED = 1;

  public static final int CALLBACK_ERROR_ALREADY_INITIALIZED = 2;

  public static final int CALLBACK_ERROR_LOAD_IN_PROGRESS = 3;

  public static final int CALLBACK_ERROR_INTERNAL_ERROR = 4;

  public static final int CALLBACK_ERROR_INVALID_REQUEST = 5;

  public static final int CALLBACK_ERROR_NETWORK_ERROR = 6;

  public static final int CALLBACK_ERROR_NO_FILL = 7;

  public static final int CALLBACK_ERROR_NO_WINDOW_TOKEN = 8;

  public static final int CALLBACK_ERROR_UNKNOWN = 9;
  // LINT.ThenChange(//depot_firebase_cpp/admob/client/cpp/src/include/firebase/admob/types.h)

  /**
   * Error messages used for completing futures. These match the error codes in the AdMobError
   * enumeration in the C++ API.
   */
  // LINT.IfChange
  public static final String CALLBACK_ERROR_MESSAGE_NONE = "";

  public static final String CALLBACK_ERROR_MESSAGE_UNINITIALIZED =
      "Ad has not been fully initialized.";

  public static final String CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED =
      "Ad is already initialized.";

  public static final String CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS = "Ad is currently loading.";

  public static final String CALLBACK_ERROR_MESSAGE_INTERNAL_ERROR =
      "An internal SDK error occurred.";

  public static final String CALLBACK_ERROR_MESSAGE_INVALID_REQUEST = "Invalid ad request.";

  public static final String CALLBACK_ERROR_MESSAGE_NETWORK_ERROR = "A network error occurred.";

  public static final String CALLBACK_ERROR_MESSAGE_NO_FILL = "No ad was available to serve.";

  public static final String CALLBACK_ERROR_MESSAGE_NO_WINDOW_TOKEN =
      "Android Activity does not have a window token.";

  public static final String CALLBACK_ERROR_MESSAGE_UNKNOWN = "Unknown error occurred.";
  // LINT.ThenChange(//depot_firebase_cpp/admob/client/cpp/src/include/firebase/admob/types.h)

  /** Types of notifications to send back to the C++ side for listeners updates. */
  // LINT.IfChange
  public static final int AD_VIEW_CHANGED_PRESENTATION_STATE = 0;

  public static final int AD_VIEW_CHANGED_BOUNDING_BOX = 1;
  // LINT.ThenChange(//depot_firebase_cpp/admob/client/cpp/src_java/com/google/firebase/admob/internal/cpp/BannerViewHelper.java,
  // //depot_firebase_cpp/admob/client/cpp/src_java/com/google/firebase/admob/internal/cpp/NativeExpressAdViewHelper.java)

  /**
   * Presentation states (matches the BannerView::PresentationState and
   * NativeExpressAdView::PresentationState enumerations in the public C++ API).
   */
  // LINT.IfChange
  public static final int AD_VIEW_PRESENTATION_STATE_HIDDEN = 0;

  public static final int AD_VIEW_PRESENTATION_STATE_VISIBLE_WITHOUT_AD = 1;

  public static final int AD_VIEW_PRESENTATION_STATE_VISIBLE_WITH_AD = 2;

  public static final int AD_VIEW_PRESENTATION_STATE_OPENED_PARTIAL_OVERLAY = 3;

  public static final int AD_VIEW_PRESENTATION_STATE_COVERING_UI = 4;
  // LINT.ThenChange(//depot_firebase_cpp/admob/client/cpp/src/include/firebase/admob/banner_view.h,
  // //depot_firebase_cpp/admob/client/cpp/src/include/firebase/admob/native_express_ad_view.h)

  /**
   * Ad view positions (matches the BannerView::Position and NativeExpressAdView::Position
   * enumerations in the public C++ API).
   */
  // LINT.IfChange
  public static final int AD_VIEW_POSITION_TOP = 0;

  public static final int AD_VIEW_POSITION_BOTTOM = 1;

  public static final int AD_VIEW_POSITION_TOP_LEFT = 2;

  public static final int AD_VIEW_POSITION_TOP_RIGHT = 3;

  public static final int AD_VIEW_POSITION_BOTTOM_LEFT = 4;

  public static final int AD_VIEW_POSITION_BOTTOM_RIGHT = 5;
  // LINT.ThenChange(//depot_firebase_cpp/admob/client/cpp/src/include/firebase/admob/banner_view.h,
  // //depot_firebase_cpp/admob/client/cpp/src/include/firebase/admob/native_express_ad_view.h)

}
