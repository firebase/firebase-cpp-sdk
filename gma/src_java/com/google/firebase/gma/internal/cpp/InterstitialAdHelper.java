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

import android.app.Activity;
import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdValue;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.OnPaidEventListener;
import com.google.android.gms.ads.ResponseInfo;
import com.google.android.gms.ads.interstitial.InterstitialAd;
import com.google.android.gms.ads.interstitial.InterstitialAdLoadCallback;

/**
 * Helper class to make interactions between the GMA C++ wrapper and Java {@link InterstitialAd}
 * objects cleaner. It's designed to wrap and adapt a single instance of {@link InterstitialAd},
 * translate calls coming from C++ into their (typically more complicated) Java equivalents.
 */
public class InterstitialAdHelper {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Pointer to the InterstitialAdInternalAndroid object that created this
  // object.
  private long interstitialAdInternalPtr;

  // The GMA SDK {@link InterstitialAd} associated with this helper.
  private InterstitialAd interstitial;

  // Synchronization object for thread safe access to:
  // * interstitial
  // * interstitialAdInternalPtr
  // * loadAdCallbackDataPtr
  private final Object interstitialLock;

  // The {@link Activity} this helper uses to display its
  // {@link InterstitialAd}.
  private Activity activity;

  // The ad unit ID to use for the {@link InterstitialAd}.
  private String adUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long loadAdCallbackDataPtr;

  /** Constructor. */
  public InterstitialAdHelper(long interstitialAdInternalPtr) {
    this.interstitialAdInternalPtr = interstitialAdInternalPtr;
    interstitialLock = new Object();

    // Test the callbacks and fail quickly if something's wrong.
    completeInterstitialAdFutureCallback(
        CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  /**
   * Initializes the {@link InterstitialAd}. This creates the corresponding GMA SDK {@link
   * InterstitialAd} object and sets it up.
   */
  public void initialize(final long callbackDataPtr, Activity activity) {
    this.activity = activity;

    this.activity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            int errorCode;
            String errorMessage;
            if (interstitial == null) {
              try {
                errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
              } catch (IllegalStateException e) {
                interstitial = null;
                // This exception can be thrown if the ad unit ID was already set.
                errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
              }
            } else {
              errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
            }
            completeInterstitialAdFutureCallback(callbackDataPtr, errorCode, errorMessage);
          }
        });
  }

  /** Disconnect the helper from the interstital ad. */
  public void disconnect() {
    synchronized (interstitialLock) {
      interstitialAdInternalPtr = CPP_NULLPTR;
      loadAdCallbackDataPtr = CPP_NULLPTR;
    }

    if (activity == null) {
      return;
    }

    activity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            synchronized (interstitialLock) {
              if (interstitial != null) {
                interstitial.setFullScreenContentCallback(null);
                interstitial.setOnPaidEventListener(null);
                interstitial = null;
              }
            }
          }
        });
  }

  /** Loads an ad for the underlying {@link InterstitialAd} object. */
  public void loadAd(long callbackDataPtr, String adUnitId, final AdRequest request) {
    if (activity == null) {
      return;
    }
    synchronized (interstitialLock) {
      if (loadAdCallbackDataPtr != CPP_NULLPTR) {
        completeInterstitialLoadAdInternalError(
            callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }
      loadAdCallbackDataPtr = callbackDataPtr;
    }

    this.adUnitId = adUnitId;

    activity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            if (activity == null) {
              synchronized (interstitialLock) {
                completeInterstitialLoadAdInternalError(
                    loadAdCallbackDataPtr,
                    ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                    ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
                loadAdCallbackDataPtr = CPP_NULLPTR;
              }
            } else {
              try {
                InterstitialAd.load(
                    activity,
                    InterstitialAdHelper.this.adUnitId,
                    request,
                    new InterstitialAdListener());
              } catch (IllegalStateException e) {
                synchronized (interstitialLock) {
                  completeInterstitialLoadAdInternalError(
                      loadAdCallbackDataPtr,
                      ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                      ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
                  loadAdCallbackDataPtr = CPP_NULLPTR;
                }
              }
            }
          }
        });
  }

  /** Shows a previously loaded ad. */
  public void show(final long callbackDataPtr) {
    activity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            synchronized (interstitialLock) {
              int errorCode;
              String errorMessage;
              if (adUnitId == null) {
                errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
              } else if (interstitial == null) {
                errorCode = ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS;
              } else {
                errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
                interstitial.show(activity);
              }
              completeInterstitialAdFutureCallback(callbackDataPtr, errorCode, errorMessage);
            }
          }
        });
  }

  private class InterstitialAdFullScreenContentListener extends FullScreenContentCallback
      implements OnPaidEventListener {
    @Override
    public void onAdClicked() {
      synchronized (interstitialLock) {
        if (interstitialAdInternalPtr != CPP_NULLPTR) {
          notifyAdClickedFullScreenContentEvent(interstitialAdInternalPtr);
        }
      }
    }

    @Override
    public void onAdDismissedFullScreenContent() {
      synchronized (interstitialLock) {
        if (interstitialAdInternalPtr != CPP_NULLPTR) {
          notifyAdDismissedFullScreenContentEvent(interstitialAdInternalPtr);
        }
      }
    }

    @Override
    public void onAdFailedToShowFullScreenContent(AdError error) {
      synchronized (interstitialLock) {
        if (interstitialAdInternalPtr != CPP_NULLPTR) {
          notifyAdFailedToShowFullScreenContentEvent(interstitialAdInternalPtr, error);
        }
      }
    }

    @Override
    public void onAdImpression() {
      synchronized (interstitialLock) {
        if (interstitialAdInternalPtr != CPP_NULLPTR) {
          notifyAdImpressionEvent(interstitialAdInternalPtr);
        }
      }
    }

    @Override
    public void onAdShowedFullScreenContent() {
      synchronized (interstitialLock) {
        if (interstitialAdInternalPtr != CPP_NULLPTR) {
          notifyAdShowedFullScreenContentEvent(interstitialAdInternalPtr);
        }
      }
    }

    @Override
    public void onPaidEvent(AdValue value) {
      synchronized (interstitialLock) {
        notifyPaidEvent(
            interstitialAdInternalPtr,
            value.getCurrencyCode(),
            value.getPrecisionType(),
            value.getValueMicros());
      }
    }
  }

  private class InterstitialAdListener extends InterstitialAdLoadCallback {
    @Override
    public void onAdFailedToLoad(LoadAdError loadAdError) {
      synchronized (interstitialLock) {
        if (loadAdCallbackDataPtr != CPP_NULLPTR) {
          completeInterstitialLoadAdError(
              loadAdCallbackDataPtr, loadAdError, loadAdError.getCode(), loadAdError.getMessage());
          loadAdCallbackDataPtr = CPP_NULLPTR;
        }
      }
    }

    @Override
    public void onAdLoaded(InterstitialAd ad) {
      synchronized (interstitialLock) {
        interstitial = ad;
        InterstitialAdFullScreenContentListener listener =
            new InterstitialAdFullScreenContentListener();
        interstitial.setFullScreenContentCallback(listener);
        interstitial.setOnPaidEventListener(listener);
        if (loadAdCallbackDataPtr != CPP_NULLPTR) {
          completeInterstitialLoadedAd(loadAdCallbackDataPtr, ad.getResponseInfo());
          loadAdCallbackDataPtr = CPP_NULLPTR;
        }
      }
    }
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeInterstitialAdFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /** Native callback invoked upon successfully loading an ad. */
  public static native void completeInterstitialLoadedAd(
      long nativeInternalPtr, ResponseInfo responseInfo);

  /**
   * Native callback upon encountering an error loading an Ad Request. Returns Android Google Mobile
   * Ads SDK error codes.
   */
  public static native void completeInterstitialLoadAdError(
      long nativeInternalPtr, LoadAdError error, int errorCode, String errorMessage);

  /**
   * Native callback upon encountering a wrapper/internal error when processing an Ad Request.
   * Returns integers representing firebase::gma::AdError codes.
   */
  public static native void completeInterstitialLoadAdInternalError(
      long nativeInternalPtr, int gmaErrorCode, String errorMessage);

  /** Native callbacks to notify the C++ wrapper of ad events */
  public static native void notifyAdClickedFullScreenContentEvent(long nativeInternalPtr);

  public static native void notifyAdDismissedFullScreenContentEvent(long nativeInternalPtr);

  public static native void notifyAdFailedToShowFullScreenContentEvent(
      long nativeInternalPtr, AdError adError);

  public static native void notifyAdImpressionEvent(long nativeInternalPtr);

  public static native void notifyAdShowedFullScreenContentEvent(long nativeInternalPtr);

  public static native void notifyPaidEvent(
      long nativeInternalPtr, String currencyCode, int precisionType, long valueMicros);
}
