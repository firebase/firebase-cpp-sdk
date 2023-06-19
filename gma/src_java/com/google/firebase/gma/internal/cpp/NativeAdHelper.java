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

package com.google.firebase.gma.internal.cpp;

import android.app.Activity;
import com.google.android.gms.ads.AdListener;
import com.google.android.gms.ads.AdLoader;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.ResponseInfo;
import com.google.android.gms.ads.nativead.NativeAd;
import java.util.List;

/**
 * Helper class to make interactions between the GMA C++ wrapper and Java objects cleaner. It's
 * designed to wrap and adapt a single instance of NativeAd, translate calls coming from C++ into
 * their (typically more complicated) Java equivalents.
 */
public class NativeAdHelper {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Pointer to the NativeAdInternalAndroid object that created this
  // object.
  private long mNativeAdInternalPtr;

  // The GMA SDK NativeAd associated with this helper.
  private NativeAd mNative;

  // Synchronization object for thread safe access to:
  // * mNative
  // * mNativeAdInternalPtr
  // * mLoadAdCallbackDataPtr
  private final Object mNativeLock;

  // The Activity this helper uses to display its NativeAd.
  private Activity mActivity;

  // The ad unit ID to use for the NativeAd.
  private String mAdUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long mLoadAdCallbackDataPtr;

  /** Constructor. */
  public NativeAdHelper(long nativeAdInternalPtr) {
    mNativeAdInternalPtr = nativeAdInternalPtr;
    mNativeLock = new Object();

    // Test the callbacks and fail quickly if something's wrong.
    completeNativeAdFutureCallback(CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  /**
   * Initializes the NativeAd. This creates the corresponding GMA SDK NativeAd object and sets it
   * up.
   */
  public void initialize(final long callbackDataPtr, Activity activity) {
    mActivity = activity;

    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        int errorCode;
        String errorMessage;
        if (mNative == null) {
          try {
            errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
          } catch (IllegalStateException e) {
            mNative = null;
            // This exception can be thrown if the ad unit ID was already set.
            errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
          }
        } else {
          errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
          errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
        }
        completeNativeAdFutureCallback(callbackDataPtr, errorCode, errorMessage);
      }
    });
  }

  /** Disconnect the helper from the interstital ad. */
  public void disconnect() {
    synchronized (mNativeLock) {
      mNativeAdInternalPtr = CPP_NULLPTR;
      mLoadAdCallbackDataPtr = CPP_NULLPTR;
    }

    if (mActivity == null) {
      return;
    }

    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        synchronized (mNativeLock) {
          if (mNative != null) {
            mNative = null;
          }
        }
      }
    });
  }

  /** Loads an ad for the underlying NativeAd object. */
  public void loadAd(long callbackDataPtr, String adUnitId, final AdRequest request) {
    if (mActivity == null) {
      return;
    }
    synchronized (mNativeLock) {
      if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
        completeNativeLoadAdInternalError(callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }
      mLoadAdCallbackDataPtr = callbackDataPtr;
    }

    mAdUnitId = adUnitId;

    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (mActivity == null) {
          synchronized (mNativeLock) {
            completeNativeLoadAdInternalError(mLoadAdCallbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
            mLoadAdCallbackDataPtr = CPP_NULLPTR;
          }
        } else {
          try {
            AdLoader.Builder adLoaderBuilder = new AdLoader.Builder(mActivity, mAdUnitId);
            NativeAdListener listener = new NativeAdListener();
            adLoaderBuilder.forNativeAd(listener);
            adLoaderBuilder.withAdListener(listener);
            AdLoader adLoader = adLoaderBuilder.build();
            adLoader.loadAd(request);
          } catch (IllegalStateException e) {
            synchronized (mNativeLock) {
              completeNativeLoadAdInternalError(mLoadAdCallbackDataPtr,
                  ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                  ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
              mLoadAdCallbackDataPtr = CPP_NULLPTR;
            }
          }
        }
      }
    });
  }

  private class NativeAdListener extends AdListener implements NativeAd.OnNativeAdLoadedListener {
    @Override
    public void onAdFailedToLoad(LoadAdError loadAdError) {
      synchronized (mNativeLock) {
        if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
          completeNativeLoadAdError(
              mLoadAdCallbackDataPtr, loadAdError, loadAdError.getCode(), loadAdError.getMessage());
          mLoadAdCallbackDataPtr = CPP_NULLPTR;
        }
      }
    }

    public void onNativeAdLoaded(NativeAd ad) {
      synchronized (mNativeLock) {
        mNative = ad;
        if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
          List<NativeAd.Image> imgList = ad.getImages();
          NativeAd.Image[] imgArray = new NativeAd.Image[imgList.size()];
          imgArray = imgList.toArray(imgArray);

          completeNativeLoadedAd(mLoadAdCallbackDataPtr, mNativeAdInternalPtr, ad.getIcon(),
              imgArray, ad.getResponseInfo());
          mLoadAdCallbackDataPtr = CPP_NULLPTR;
        }
      }
    }
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeNativeAdFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /** Native callback invoked upon successfully loading an ad. */
  public static native void completeNativeLoadedAd(long nativeInternalPtr,
      long mNativeAdInternalPtr, NativeAd.Image icon, NativeAd.Image[] images,
      ResponseInfo responseInfo);

  /**
   * Native callback upon encountering an error loading an Ad Request. Returns Android Google Mobile
   * Ads SDK error codes.
   */
  public static native void completeNativeLoadAdError(
      long nativeInternalPtr, LoadAdError error, int errorCode, String errorMessage);

  /**
   * Native callback upon encountering a wrapper/internal error when processing an Ad Request.
   * Returns integers representing firebase::gma::AdError codes.
   */
  public static native void completeNativeLoadAdInternalError(
      long nativeInternalPtr, int gmaErrorCode, String errorMessage);
}
