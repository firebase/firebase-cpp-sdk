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
import com.google.android.gms.ads.OnUserEarnedRewardListener;
import com.google.android.gms.ads.ResponseInfo;
import com.google.android.gms.ads.rewarded.RewardItem;
import com.google.android.gms.ads.rewarded.RewardedAd;
import com.google.android.gms.ads.rewarded.RewardedAdLoadCallback;
import com.google.android.gms.ads.rewarded.ServerSideVerificationOptions;

/**
 * Helper class to make interactions between the GMA C++ wrapper and Java {@link RewardedAd} objects
 * cleaner. It's designed to wrap and adapt a single instance of {@link RewardedAd}, translate calls
 * coming from C++ into their (typically more complicated) Java equivalents.
 */
public class RewardedAdHelper {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Pointer to the RewardedAdInternalAndroid object that created this
  // object.
  private long rewardedAdInternalPtr;

  // The GMA SDK {@link RewardedAd} associated with this helper.
  private RewardedAd rewarded;

  // Synchronization object for thread safe access to:
  // * rewarded
  // * rewardedAdInternalPtr
  // * loadAdCallbackDataPtr
  private final Object rewardedLock;

  // The {@link Activity} this helper uses to display its
  // {@link RewardedAd}.
  private Activity activity;

  // The ad unit ID to use for the {@link RewardedAd}.
  private String adUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long loadAdCallbackDataPtr;

  /** Constructor. */
  public RewardedAdHelper(long rewardedAdInternalPtr) {
    this.rewardedAdInternalPtr = rewardedAdInternalPtr;
    rewardedLock = new Object();

    // Test the callbacks and fail quickly if something's wrong.
    completeRewardedAdFutureCallback(CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  /**
   * Initializes the {@link RewardedAd}. This creates the corresponding GMA SDK {@link RewardedAd}
   * object and sets it up.
   */
  public void initialize(final long callbackDataPtr, Activity activity) {
    this.activity = activity;

    this.activity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        int errorCode;
        String errorMessage;
        synchronized (rewardedLock) {
          if (rewarded == null) {
            try {
              errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
            } catch (IllegalStateException e) {
              rewarded = null;
              // This exception can be thrown if the ad unit ID was already set.
              errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
            }
          } else {
            errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
          }
          completeRewardedAdFutureCallback(callbackDataPtr, errorCode, errorMessage);
        }
      }
    });
  }

  /** Disconnect the helper from the interstital ad. */
  public void disconnect() {
    synchronized (rewardedLock) {
      rewardedAdInternalPtr = CPP_NULLPTR;
      loadAdCallbackDataPtr = CPP_NULLPTR;
    }

    if (activity == null) {
      return;
    }

    activity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        synchronized (rewardedLock) {
          if (rewarded != null) {
            rewarded.setFullScreenContentCallback(null);
            rewarded.setOnPaidEventListener(null);
            rewarded = null;
          }
        }
      }
    });
  }

  /** Loads an ad for the underlying {@link RewardedAd} object. */
  public void loadAd(long callbackDataPtr, String adUnitId, final AdRequest request) {
    if (activity == null) {
      return;
    }
    synchronized (rewardedLock) {
      if (loadAdCallbackDataPtr != CPP_NULLPTR) {
        completeRewardedLoadAdInternalError(callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }
      loadAdCallbackDataPtr = callbackDataPtr;
    }

    this.adUnitId = adUnitId;

    activity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        synchronized (rewardedLock) {
          if (activity == null) {
            completeRewardedLoadAdInternalError(loadAdCallbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
            loadAdCallbackDataPtr = CPP_NULLPTR;
          } else {
            try {
              RewardedAd.load(
                  activity, RewardedAdHelper.this.adUnitId, request, new RewardedAdListener());
            } catch (IllegalStateException e) {
              completeRewardedLoadAdInternalError(loadAdCallbackDataPtr,
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
  public void show(final long callbackDataPtr, final String verificationCustomData,
      final String verificationUserId) {
    activity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        synchronized (rewardedLock) {
          int errorCode;
          String errorMessage;
          if (adUnitId == null) {
            errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
          } else if (rewarded == null) {
            errorCode = ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS;
          } else {
            errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
            if (!verificationCustomData.isEmpty() || !verificationUserId.isEmpty()) {
              ServerSideVerificationOptions options = new ServerSideVerificationOptions.Builder()
                                                          .setCustomData(verificationCustomData)
                                                          .setUserId(verificationUserId)
                                                          .build();
              rewarded.setServerSideVerificationOptions(options);
            }
            rewarded.show(activity, new UserEarnedRewardListener());
          }
          completeRewardedAdFutureCallback(callbackDataPtr, errorCode, errorMessage);
        }
      }
    });
  }

  private class UserEarnedRewardListener implements OnUserEarnedRewardListener {
    @Override
    public void onUserEarnedReward(RewardItem rewardItem) {
      synchronized (rewardedLock) {
        if (rewardedAdInternalPtr != CPP_NULLPTR) {
          notifyUserEarnedRewardEvent(
              rewardedAdInternalPtr, rewardItem.getType(), rewardItem.getAmount());
        }
      }
    }
  }

  private class RewardedAdFullScreenContentListener
      extends FullScreenContentCallback implements OnPaidEventListener {
    @Override
    public void onAdClicked() {
      synchronized (rewardedLock) {
        if (rewardedAdInternalPtr != CPP_NULLPTR) {
          notifyAdClickedFullScreenContentEvent(rewardedAdInternalPtr);
        }
      }
    }

    @Override
    public void onAdDismissedFullScreenContent() {
      synchronized (rewardedLock) {
        if (rewardedAdInternalPtr != CPP_NULLPTR) {
          notifyAdDismissedFullScreenContentEvent(rewardedAdInternalPtr);
        }
      }
    }

    @Override
    public void onAdFailedToShowFullScreenContent(AdError error) {
      synchronized (rewardedLock) {
        if (rewardedAdInternalPtr != CPP_NULLPTR) {
          notifyAdFailedToShowFullScreenContentEvent(rewardedAdInternalPtr, error);
        }
      }
    }

    @Override
    public void onAdImpression() {
      synchronized (rewardedLock) {
        if (rewardedAdInternalPtr != CPP_NULLPTR) {
          notifyAdImpressionEvent(rewardedAdInternalPtr);
        }
      }
    }

    @Override
    public void onAdShowedFullScreenContent() {
      synchronized (rewardedLock) {
        if (rewardedAdInternalPtr != CPP_NULLPTR) {
          notifyAdShowedFullScreenContentEvent(rewardedAdInternalPtr);
        }
      }
    }

    @Override
    public void onPaidEvent(AdValue value) {
      synchronized (rewardedLock) {
        if (rewardedAdInternalPtr != CPP_NULLPTR) {
          notifyPaidEvent(rewardedAdInternalPtr, value.getCurrencyCode(), value.getPrecisionType(),
              value.getValueMicros());
        }
      }
    }
  }

  private class RewardedAdListener extends RewardedAdLoadCallback {
    @Override
    public void onAdFailedToLoad(LoadAdError loadAdError) {
      synchronized (rewardedLock) {
        if (loadAdCallbackDataPtr != CPP_NULLPTR) {
          completeRewardedLoadAdError(
              loadAdCallbackDataPtr, loadAdError, loadAdError.getCode(), loadAdError.getMessage());
          loadAdCallbackDataPtr = CPP_NULLPTR;
        }
      }
    }

    @Override
    public void onAdLoaded(RewardedAd ad) {
      synchronized (rewardedLock) {
        if (loadAdCallbackDataPtr != CPP_NULLPTR) {
          rewarded = ad;
          RewardedAdFullScreenContentListener listener = new RewardedAdFullScreenContentListener();
          rewarded.setFullScreenContentCallback(listener);
          rewarded.setOnPaidEventListener(listener);
          completeRewardedLoadedAd(loadAdCallbackDataPtr, rewarded.getResponseInfo());
          loadAdCallbackDataPtr = CPP_NULLPTR;
        }
      }
    }
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeRewardedAdFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /** Native callback invoked upon successfully loading an ad. */
  public static native void completeRewardedLoadedAd(
      long nativeInternalPtr, ResponseInfo responseInfo);

  /**
   * Native callback upon encountering an error loading an Ad Request. Returns Android Google Mobile
   * Ads SDK error codes.
   */
  public static native void completeRewardedLoadAdError(
      long nativeInternalPtr, LoadAdError error, int errorCode, String errorMessage);

  /**
   * Native callback upon encountering a wrapper/internal error when processing an Ad Request.
   * Returns integers representing firebase::gma::AdError codes.
   */
  public static native void completeRewardedLoadAdInternalError(
      long nativeInternalPtr, int gmaErrorCode, String errorMessage);

  /** Native callbacks to notify the C++ wrapper of ad events */
  public static native void notifyUserEarnedRewardEvent(
      long mRewardedAdInternalPtr, String type, int amount);

  public static native void notifyAdClickedFullScreenContentEvent(long nativeInternalPtr);

  public static native void notifyAdDismissedFullScreenContentEvent(long nativeInternalPtr);

  public static native void notifyAdFailedToShowFullScreenContentEvent(
      long nativeInternalPtr, AdError adError);

  public static native void notifyAdImpressionEvent(long nativeInternalPtr);

  public static native void notifyAdShowedFullScreenContentEvent(long nativeInternalPtr);

  public static native void notifyPaidEvent(
      long nativeInternalPtr, String currencyCode, int precisionType, long valueMicros);
}
