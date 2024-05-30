/*
 * Copyright 2024 Google LLC
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

import android.app.Activity;
import com.google.android.gms.ads.AdFormat;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.query.QueryInfo;
import com.google.android.gms.ads.query.QueryInfoGenerationCallback;

/**
 * Helper class to make interactions between the GMA C++ wrapper and Java objects cleaner. It's
 * designed to wrap and adapt a single instance of QueryInfo, translate calls coming from C++ into
 * their (typically more complicated) Java equivalents.
 */
public class QueryInfoHelper {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Pointer to the QueryInfoInternalAndroid object that created this
  // object.
  private long queryInfoInternalPtr;

  // The GMA SDK QueryInfo associated with this helper.
  private QueryInfo gmaQueryInfo;

  // Synchronization object for thread safe access to:
  // * queryInfoInternalPtr
  // * createQueryInfoCallbackDataPtr
  private final Object queryInfoLock;

  // The Activity this helper uses to generate the QueryInfo.
  private Activity activity;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to CreateQueryInfo.
  private long createQueryInfoCallbackDataPtr;

  /** Constructor. */
  public QueryInfoHelper(long queryInfoInternalPtr) {
    this.queryInfoInternalPtr = queryInfoInternalPtr;
    queryInfoLock = new Object();

    // Test the callbacks and fail quickly if something's wrong.
    completeQueryInfoFutureCallback(CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  /**
   * Initializes the QueryInfoHelper. This creates the corresponding GMA SDK NativeAd object and
   * sets it up.
   */
  public void initialize(final long callbackDataPtr, Activity activity) {
    this.activity = activity;

    this.activity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        int errorCode;
        String errorMessage;
        if (gmaQueryInfo == null) {
          try {
            errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
          } catch (IllegalStateException e) {
            gmaQueryInfo = null;
            errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
          }
        } else {
          errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
          errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
        }
        completeQueryInfoFutureCallback(callbackDataPtr, errorCode, errorMessage);
      }
    });
  }

  /** Disconnect the helper from the query info. */
  public void disconnect() {
    synchronized (queryInfoLock) {
      queryInfoInternalPtr = CPP_NULLPTR;
      createQueryInfoCallbackDataPtr = CPP_NULLPTR;
    }

    if (activity == null) {
      return;
    }

    activity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        synchronized (queryInfoLock) {
          if (gmaQueryInfo != null) {
            gmaQueryInfo = null;
          }
        }
      }
    });
  }

  private AdFormat getAdFormat(int format) {
    switch (format) {
      case ConstantsHelper.AD_FORMAT_BANNER:
        return AdFormat.BANNER;
      case ConstantsHelper.AD_FORMAT_INTERSTITIAL:
        return AdFormat.INTERSTITIAL;
      case ConstantsHelper.AD_FORMAT_REWARDED:
        return AdFormat.REWARDED;
      case ConstantsHelper.AD_FORMAT_NATIVE:
        return AdFormat.NATIVE;
      case ConstantsHelper.AD_FORMAT_REWARDED_INTERSTITIAL:
        return AdFormat.REWARDED_INTERSTITIAL;
      default:
        return AdFormat.APP_OPEN_AD;
    }
  }

  /** Creates a query info for the underlying QueryInfo object. */
  public void createQueryInfo(
      long callbackDataPtr, int format, String adUnitId, final AdRequest request) {
    if (activity == null) {
      return;
    }
    synchronized (queryInfoLock) {
      if (createQueryInfoCallbackDataPtr != CPP_NULLPTR) {
        completeCreateQueryInfoError(callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }
      createQueryInfoCallbackDataPtr = callbackDataPtr;
    }

    activity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (activity == null) {
          synchronized (queryInfoLock) {
            completeCreateQueryInfoError(createQueryInfoCallbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
            createQueryInfoCallbackDataPtr = CPP_NULLPTR;
          }
        } else {
          try {
            AdFormat adFormat = getAdFormat(format);
            if (adUnitId != null && !adUnitId.isEmpty()) {
              QueryInfo.generate(activity, adFormat, request, adUnitId, new QueryInfoListener());
            } else {
              QueryInfo.generate(activity, adFormat, request, new QueryInfoListener());
            }
          } catch (IllegalStateException e) {
            synchronized (queryInfoLock) {
              completeCreateQueryInfoError(createQueryInfoCallbackDataPtr,
                  ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                  ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
              createQueryInfoCallbackDataPtr = CPP_NULLPTR;
            }
          }
        }
      }
    });
  }

  private class QueryInfoListener extends QueryInfoGenerationCallback {
    @Override
    public void onFailure(String errorMessage) {
      synchronized (queryInfoLock) {
        if (createQueryInfoCallbackDataPtr != CPP_NULLPTR) {
          completeCreateQueryInfoError(createQueryInfoCallbackDataPtr,
              ConstantsHelper.CALLBACK_ERROR_INVALID_REQUEST, errorMessage);
          createQueryInfoCallbackDataPtr = CPP_NULLPTR;
        }
      }
    }

    @Override
    public void onSuccess(QueryInfo queryInfo) {
      synchronized (queryInfoLock) {
        gmaQueryInfo = queryInfo;
        if (createQueryInfoCallbackDataPtr != CPP_NULLPTR) {
          completeCreateQueryInfoSuccess(createQueryInfoCallbackDataPtr, queryInfo.getQuery());
          createQueryInfoCallbackDataPtr = CPP_NULLPTR;
        }
      }
    }
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeQueryInfoFutureCallback(
      long internalPtr, int errorCode, String errorMessage);

  /** Native callback invoked upon successfully generating a QueryInfo. */
  public static native void completeCreateQueryInfoSuccess(
      long createQueryInfoInternalPtr, String query);

  /**
   * Native callback invoked upon error generating a QueryInfo. Also used for wrapper/internal
   * errors when processing a query info generation request. Returns integers representing
   * firebase::gma::AdError codes.
   */
  public static native void completeCreateQueryInfoError(
      long createQueryInfoInternalPtr, int gmaErrorCode, String errorMessage);
}
