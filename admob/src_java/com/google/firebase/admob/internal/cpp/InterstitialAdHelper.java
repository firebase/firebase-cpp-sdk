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

import android.app.Activity;

import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.interstitial.InterstitialAd;
import com.google.android.gms.ads.interstitial.InterstitialAdLoadCallback;

import android.util.Log;

/**
 * Helper class to make interactions between the AdMob C++ wrapper and Java {@link InterstitialAd}
 * objects cleaner. It's designed to wrap and adapt a single instance of {@link InterstitialAd},
 * translate calls coming from C++ into their (typically more complicated) Java equivalents, and
 * convert the Java listener patterns into game engine-friendly state machine polling.
 */
public class InterstitialAdHelper {

  // Presentation states (matches the InterstitialAd::PresentationState
  // enumeration in the public C++ API).
  // LINT.IfChange
  public static final int PRESENTATION_STATE_HIDDEN = 0;
  public static final int PRESENTATION_STATE_COVERING_UI = 1;
  // LINT.ThenChange(//depot_firebase_cpp/admob/client/cpp/src/include/firebase/admob/interstitial_ad.h)

  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Pointer to the InterstitialAdInternalAndroid object that created this
  // object.
  private long mInterstitialAdInternalPtr;

  // The GMA SDK {@link InterstitialAd} associated with this helper.
  private InterstitialAd mInterstitial;

  // Synchronization object for thread safe access to:
  // * mInterstitial
  // * mInterstitialAdInternalPtr
  // * mLoadAdCallbackDataPtr
  private final Object mInterstitialLock;

  // The current presentation state.
  private int mCurrentPresentationState;

  // The {@link Activity} this helper uses to display its
  // {@link InterstitialAd}.
  private Activity mActivity;

  // The ad unit ID to use for the {@link InterstitialAd}.
  private String mAdUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long mLoadAdCallbackDataPtr;

  /** Constructor. */
  public InterstitialAdHelper(long interstitialAdInternalPtr) {
    mInterstitialAdInternalPtr = interstitialAdInternalPtr;
    mCurrentPresentationState = PRESENTATION_STATE_HIDDEN;
    mInterstitialLock = new Object();

    // Test the callbacks and fail quickly if something's wrong.
    completeInterstitialAdFutureCallback(
        CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
    notifyPresentationStateChanged(CPP_NULLPTR, mCurrentPresentationState);
  }

  /**
   * Initializes the {@link InterstitialAd}. This creates the corresponding GMA SDK {@link
   * InterstitialAd} object and sets it up.
   */
  public void initialize(final long callbackDataPtr, Activity activity) {
    mActivity = activity;

    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            int errorCode;
            String errorMessage;
            if (mInterstitial == null) {
              try {
                errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
              } catch (IllegalStateException e) {
                mInterstitial = null;
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
    synchronized (mInterstitialLock) {
      mInterstitialAdInternalPtr = CPP_NULLPTR;
      mLoadAdCallbackDataPtr = CPP_NULLPTR;
    }
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            synchronized (mInterstitialLock) {
              if( mInterstitial != null) {
                mInterstitial.setFullScreenContentCallback(null);
                mInterstitial.setOnPaidEventListener(null);
                mInterstitial = null;
              }
            }
          }
        });
  }

  /** Loads an ad for the underlying {@link InterstitialAd} object. */
  public void loadAd(long callbackDataPtr, String adUnitId, final AdRequest request) {
    if(mActivity == null) {
      return;
    }
    synchronized (mInterstitialLock) {
      if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
        completeInterstitialLoadAdInternalError(
          callbackDataPtr,
          ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
          ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }
      mLoadAdCallbackDataPtr = callbackDataPtr;
    }

    mAdUnitId = adUnitId;

    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            if (mActivity == null) {
              synchronized (mInterstitialLock) {
                completeInterstitialLoadAdInternalError(
                  mLoadAdCallbackDataPtr,
                  ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                  ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
                mLoadAdCallbackDataPtr = CPP_NULLPTR;
              }
            } else {
              try {
                InterstitialAd.load(mActivity, mAdUnitId, request, new InterstitialAdListener());
              } catch (IllegalStateException e) {
                synchronized (mInterstitialLock) {
                  completeInterstitialLoadAdInternalError(
                    mLoadAdCallbackDataPtr,
                    ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                    ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
                  mLoadAdCallbackDataPtr = CPP_NULLPTR;
                }
              }
            }
          }
        });
  }

  /** Shows a previously loaded ad. */
  public void show(final long callbackDataPtr) {
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            synchronized (mInterstitialLock) {
              int errorCode;
              String errorMessage;
              if (mAdUnitId == null) {
                errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
              } else if (mInterstitial == null) {
                errorCode = ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS;
              } else {
                errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
                mInterstitial.show(mActivity);
              }
              completeInterstitialAdFutureCallback(callbackDataPtr, errorCode, errorMessage);
            }
          }
        });
  }

  /** Retrieves the presentation state of this InterstitialAdHelper. */
  public int getPresentationState() {
    return mCurrentPresentationState;
  }

  private class InterstitialAdFullScreenContentListener extends FullScreenContentCallback {
    @Override
    public void onAdDismissedFullScreenContent() {
      synchronized (mInterstitialLock) {
        mCurrentPresentationState = PRESENTATION_STATE_HIDDEN;
        notifyPresentationStateChanged(mInterstitialAdInternalPtr, mCurrentPresentationState);
      }
    }

    @Override
    public void onAdFailedToShowFullScreenContent(AdError error) {
      // TODO. 
    }

    @Override
    public void onAdImpression() {
      // TODO. 
    }

    @Override
    public void onAdShowedFullScreenContent() {
      synchronized (mInterstitialLock) {
        mCurrentPresentationState = PRESENTATION_STATE_COVERING_UI;
        notifyPresentationStateChanged(mInterstitialAdInternalPtr, mCurrentPresentationState);
      }
    }
  }

  private class InterstitialAdListener extends InterstitialAdLoadCallback {
    @Override
    public void onAdFailedToLoad(LoadAdError loadAdError) {
      synchronized (mInterstitialLock) {
        completeInterstitialLoadAdError(mLoadAdCallbackDataPtr, loadAdError, loadAdError.getCode(), loadAdError.getMessage());
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }
    }

    @Override
    public void onAdLoaded(InterstitialAd ad) {
      synchronized (mInterstitialLock) {
        mInterstitial = ad;
        mInterstitial.setFullScreenContentCallback(new InterstitialAdFullScreenContentListener());
        completeInterstitialLoadedAd(mLoadAdCallbackDataPtr);
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }
    }
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeInterstitialAdFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /** Native callback invoked upon successfully loading an ad. */
  public static native void completeInterstitialLoadedAd(long nativeInternalPtr);

  /**
   * Native callback upon encountering an error loading an Ad Request. Returns
   * Android Admob SDK error codes.
   **/
  public static native void completeInterstitialLoadAdError(
    long nativeInternalPtr, LoadAdError error, int errorCode, String errorMessage);

  /** Native callback upon encountering a wrapper/internal error when
   * processing an Ad Request. Returns integers representing
   * firebase::admob::AdMobError codes. */
  public static native void completeInterstitialLoadAdInternalError(
    long nativeInternalPtr, int admobErrorCode, String errorMessage);

  /** Native callback to notify the C++ wrapper that a state change has occurred. */
  public static native void notifyPresentationStateChanged(long nativeInternalPtr, int state);
}
