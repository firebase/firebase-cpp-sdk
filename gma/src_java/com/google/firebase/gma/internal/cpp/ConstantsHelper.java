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

/** Helper class containing constants that are shared across the various GMA ad formats. */
public final class ConstantsHelper {
  /** Error codes used in completing futures. These match the AdError enumeration in the C++ API. */
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

  /**
   * Error messages used for completing futures. These match the error codes in the AdError
   * enumeration in the C++ API.
   */
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

  /**
   * Ad view positions (matches the AdView::Position and NativeExpressAdView::Position enumerations
   * in the public C++ API).
   */
  public static final int AD_VIEW_POSITION_UNDEFINED = -1;

  public static final int AD_VIEW_POSITION_TOP = 0;

  public static final int AD_VIEW_POSITION_BOTTOM = 1;

  public static final int AD_VIEW_POSITION_TOP_LEFT = 2;

  public static final int AD_VIEW_POSITION_TOP_RIGHT = 3;

  public static final int AD_VIEW_POSITION_BOTTOM_LEFT = 4;

  public static final int AD_VIEW_POSITION_BOTTOM_RIGHT = 5;
}
