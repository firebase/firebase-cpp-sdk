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
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.reward.RewardItem;
import com.google.android.gms.ads.reward.RewardedVideoAd;
import com.google.android.gms.ads.reward.RewardedVideoAdListener;

/**
 * Helper class to make interactions between the AdMob C++ wrapper and Java {@link RewardedVideoAd}
 * objects cleaner. It's designed to wrap and adapt the singleton instance of {@link
 * RewardedVideoAd}, translate calls coming from C++ into their (typically more complicated) Java
 * equivalents, and convert the Java listener patterns into game engine-friendly state machine
 * polling.
 *
 * <p>A number of methods in this class take a long called callbackDataPtr as their first argument.
 * These methods correspond to native methods in the Public C++ API that return a Future. The
 * callbackDataPtr points to a FutureCallbackData that's needed to complete that Future once the
 * Java method has completed its work. This is done by passing the callbackDataPtr as an arugment to
 * completeRewardedVideoFutureCallback, a native method that can do the completion. In all cases,
 * completeRewardedVideoFutureCallback must be called so the Future on the C++ side is not left in a
 * pending state.
 *
 * <p>For every method except loadAd, this can be handled within local scope. loadAd, on the other
 * hand, must maintain the FutureCallbackData pointer even after returning. This is because the
 * action of loading an ad isn't complete until the Mobile Ads SDK invokes one of two
 * RewardedVideoAdListener Methods: onRewardedVideoAdFailedToLoad and onRewardedVideoAdLoaded. The
 * pointer is therefore stored in mLoadAdCallbackDataPtr, and mRewardedVideoLock is used to keep
 * access to it threadsafe. Because the callback pointer only needs to be valid while a call is in
 * progress, mLoadAdCallbackDataPtr is also used by loadAd to determine if any previous calls are
 * still running (in which case the new call gracefully fails).
 */
public class RewardedVideoHelper {

  // Presentation states (matches the rewarded_video::PresentationState
  // enumeration in the public C++ API).
  // LINT.IfChange
  public static final int PRESENTATION_STATE_HIDDEN = 0;
  public static final int PRESENTATION_STATE_COVERING_UI = 1;
  public static final int PRESENTATION_STATE_VIDEO_HAS_STARTED = 2;
  public static final int PRESENTATION_STATE_VIDEO_HAS_COMPLETED = 3;
  // LINT.ThenChange(//depot_firebase_cpp/admob/client/cpp/src/include/firebase/admob/rewarded_video.h)

  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Default value for the reward type, used if the rewarded ad doesn't provide one.
  private static final String DEFAULT_REWARD_TYPE = "";

  // Pointer to the RewardedVideoInternalAndroid object that created this
  // object. It will be used when invoking notifyPresentationStateChanged and
  // grantReward (to update the rewarded_video::Listener that's been set on the
  // native side).
  private long mRewardedVideoInternalPtr;

  // Synchronization object for thread safe access to mLoadAdCallbackDataPtr,
  // mRewardedVideoInternalPtr, and mDisconnected.
  private final Object mRewardedVideoLock;

  // The current presentation state.
  private int mCurrentPresentationState;

  // The {@link Activity} this helper uses to display its
  // {@link RewardedVideoAd}.
  private final Activity mActivity;

  // The ad unit ID to use for the {@link RewardedVideoAd}.
  private String mAdUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long mLoadAdCallbackDataPtr;

  // Flag indicating this helper has been disconnected and should no longer send
  // callbacks. Once this has been set to true, that instance is destined for
  // GC, and no further calls should be made!
  private boolean mDisconnected;

  /**
   * Constructor.
   *
   * <p>In addition to initializing some data members, it's responsible for testing the native
   * callbacks to ensure that the JNI link from this object to the RewardedVideoInternalAndroid that
   * created it is set up and working correctly.
   *
   * @param rewardedVideoInternalPtr a native pointer to the RewardedVideoInternalAndroid object
   *     that created this instance.
   * @param activity the Android activity to be used as Context when retrieving the RewardedVideoAd
   *     singleton.
   */
  public RewardedVideoHelper(long rewardedVideoInternalPtr, Activity activity) {
    mRewardedVideoInternalPtr = rewardedVideoInternalPtr;
    mCurrentPresentationState = PRESENTATION_STATE_HIDDEN;
    mRewardedVideoLock = new Object();
    mDisconnected = false;
    mActivity = activity;

    // Test the callbacks and fail quickly if something's wrong.
    completeRewardedVideoFutureCallback(
        CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
    notifyPresentationStateChanged(CPP_NULLPTR, mCurrentPresentationState);
    grantReward(CPP_NULLPTR, 0, DEFAULT_REWARD_TYPE);
  }

  /**
   * Retrieves a reference to the GMA SDK {@link RewardedVideoAd} singleton and sets this object as
   * its listener.
   *
   * <p>Once this method has completed, the object should be considered ready for subsequent calls
   * to loadAd, show, and so on. Those methods should not be invoked until this has been called and
   * allowed to return. This method should not be invoked more than once.
   *
   * @param callbackDataPtr a pointer to the native FutureCallbackData struct associated with the
   *     call.
   */
  public void initialize(final long callbackDataPtr) {
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            RewardedVideoAd ad = MobileAds.getRewardedVideoAdInstance(mActivity);
            ad.setRewardedVideoAdListener(new RewardedListener());
            completeRewardedVideoFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
          }
        });
  }

  /**
   * Loads an ad for the underlying {@link RewardedVideoAd} object.
   *
   * <p>This method checks mLoadAdCallbackDataPtr to determine if any previous calls are still in
   * progress. It is safe to call this method concurrently on multiple threads, though the second
   * call will gracefully fail to make sure that only one call to the Mobile Ads SDK's
   * RewardedVideoAd.loadAd method is made at a time.
   *
   * @param callbackDataPtr a pointer to the native FutureCallbackData struct associated with the
   *     call.
   * @param adUnitId the AdMob ad unit ID to use in making the request.
   * @param request an AdRequest object with targeting/extra info.
   */
  public void loadAd(long callbackDataPtr, final String adUnitId, final AdRequest request) {
    synchronized (mRewardedVideoLock) {
      if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
        completeRewardedVideoFutureCallback(
            callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }

      mLoadAdCallbackDataPtr = callbackDataPtr;
    }

    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            RewardedVideoAd ad = MobileAds.getRewardedVideoAdInstance(mActivity);
            ad.loadAd(adUnitId, request);
          }
        });
  }

  /**
   * Shows a rewarded video ad, if one has been cached. This method does its work on the UI thread,
   * and is safe to call from multiple threads concurrently.
   *
   * @param callbackDataPtr a pointer to the native FutureCallbackData struct associated with the
   *     call.
   */
  public void show(final long callbackDataPtr) {
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            int errorCode;
            String errorMessage;
            RewardedVideoAd ad = MobileAds.getRewardedVideoAdInstance(mActivity);
            if (!ad.isLoaded()) {
              errorCode = ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS;
            } else {
              errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
              ad.show();
            }

            completeRewardedVideoFutureCallback(callbackDataPtr, errorCode, errorMessage);
          }
        });
  }

  /**
   * Cleans up temporary resources in advance of destroying this helper.
   *
   * <p>This method invokes the SDK's destroy() method, which in turn calls the onDestroy() method
   * for each third party mediation adapter. It also sets a flag to prevent any further callbacks
   * (beyond its own) from being made to the RewardedVideoInternalAndroid that owns this instance.
   * Once this method has been invoked, this object should be considered unusable, and no calls to
   * this or any other methods should be made.
   *
   * @param callbackDataPtr a pointer to the native FutureCallbackData struct associated with the
   *     call.
   */
  public void destroy(final long callbackDataPtr) {
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            RewardedVideoAd ad = MobileAds.getRewardedVideoAdInstance(mActivity);
            ad.destroy(mActivity);
            completeRewardedVideoFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);

            // With this set, there should be no further callbacks made to the
            // C++ side. This is synchronized to ensure that all callbacks
            // currently running are completed before this method returns.
            synchronized (mRewardedVideoLock) {
              mDisconnected = true;
            }
          }
        });
  }

  /**
   * Pauses the rewarded video system. This method does its work on the UI thread, and is safe to
   * call from multiple threads concurrently.
   *
   * @param callbackDataPtr a pointer to the native FutureCallbackData struct associated with the
   *     call.
   */
  public void pause(final long callbackDataPtr) {
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            RewardedVideoAd ad = MobileAds.getRewardedVideoAdInstance(mActivity);
            ad.pause(mActivity);
            completeRewardedVideoFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
          }
        });
  }

  /**
   * Resumes from a pause. This method does its work on the UI thread, and is safe to call from
   * multiple threads concurrently.
   *
   * @param callbackDataPtr a pointer to the native FutureCallbackData struct associated with the
   *     call.
   */
  public void resume(final long callbackDataPtr) {
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            RewardedVideoAd ad = MobileAds.getRewardedVideoAdInstance(mActivity);
            ad.resume(mActivity);
            completeRewardedVideoFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
          }
        });
  }

  /** Retrieves the presentation state of this RewardedVideoHelper. */
  public int getPresentationState() {
    return mCurrentPresentationState;
  }

  /**
   * Receives callbacks from the Mobile Ads SDK indicating events in the lifecycle of an AdMob
   * rewarded video ad. The two loading-related callbacks are responsible for resetting
   * mLoadAdCallbackDataPtr to CPP_NULLPTR so that subsequent calls to loadAd can go through. See
   * the comment on loadAd for more info.
   */
  private class RewardedListener implements RewardedVideoAdListener {
    @Override
    public void onRewardedVideoAdClosed() {
      mCurrentPresentationState = PRESENTATION_STATE_HIDDEN;
      synchronized (mRewardedVideoLock) {
        if (!mDisconnected) {
          notifyPresentationStateChanged(mRewardedVideoInternalPtr, mCurrentPresentationState);
        }
      }
    }

    @Override
    public void onRewardedVideoAdFailedToLoad(int errorCode) {
      int callbackErrorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
      String callbackErrorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
      switch (errorCode) {
        case AdRequest.ERROR_CODE_INVALID_REQUEST:
          callbackErrorCode = ConstantsHelper.CALLBACK_ERROR_INVALID_REQUEST;
          callbackErrorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_INVALID_REQUEST;
          break;
        case AdRequest.ERROR_CODE_NETWORK_ERROR:
          callbackErrorCode = ConstantsHelper.CALLBACK_ERROR_NETWORK_ERROR;
          callbackErrorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NETWORK_ERROR;
          break;
        case AdRequest.ERROR_CODE_NO_FILL:
          callbackErrorCode = ConstantsHelper.CALLBACK_ERROR_NO_FILL;
          callbackErrorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NO_FILL;
          break;
        case AdRequest.ERROR_CODE_INTERNAL_ERROR:
        default:
          callbackErrorCode = ConstantsHelper.CALLBACK_ERROR_INTERNAL_ERROR;
          callbackErrorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_INTERNAL_ERROR;
      }

      synchronized (mRewardedVideoLock) {
        completeRewardedVideoFutureCallback(
            mLoadAdCallbackDataPtr, callbackErrorCode, callbackErrorMessage);
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }
    }

    @Override
    public void onRewardedVideoAdLeftApplication() {
      // There is no matching presentation state for this one, so nothing needs to be done here.
      // onRewardedVideoAdOpened will already have been called by the SDK, so
      // mCurrentPresentationState will already be correct.
    }

    @Override
    public void onRewardedVideoAdLoaded() {
      synchronized (mRewardedVideoLock) {
        completeRewardedVideoFutureCallback(
            mLoadAdCallbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_NONE,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }
    }

    @Override
    public void onRewardedVideoAdOpened() {
      mCurrentPresentationState = PRESENTATION_STATE_COVERING_UI;
      synchronized (mRewardedVideoLock) {
        if (!mDisconnected) {
          notifyPresentationStateChanged(mRewardedVideoInternalPtr, mCurrentPresentationState);
        }
      }
    }

    @Override
    public void onRewarded(RewardItem rewardItem) {
      synchronized (mRewardedVideoLock) {
        if (!mDisconnected) {
          String rewardType = rewardItem.getType();

          // If ad units aren't set up right, they can return nulls for the reward
          // type. Better to use a default value (currently an empty string).
          if (rewardType == null) {
            rewardType = DEFAULT_REWARD_TYPE;
          }

          grantReward(mRewardedVideoInternalPtr, rewardItem.getAmount(), rewardType);
        }
      }
    }

    @Override
    public void onRewardedVideoStarted() {
      mCurrentPresentationState = PRESENTATION_STATE_VIDEO_HAS_STARTED;
      synchronized (mRewardedVideoLock) {
        if (!mDisconnected) {
          notifyPresentationStateChanged(mRewardedVideoInternalPtr, mCurrentPresentationState);
        }
      }
    }

    @Override
    public void onRewardedVideoCompleted() {
      mCurrentPresentationState = PRESENTATION_STATE_VIDEO_HAS_COMPLETED;
      synchronized (mRewardedVideoLock) {
        if (!mDisconnected) {
          notifyPresentationStateChanged(mRewardedVideoInternalPtr, mCurrentPresentationState);
        }
      }
    }
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeRewardedVideoFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /** Native callback to notify the C++ wrapper that a state change has occurred. */
  public static native void notifyPresentationStateChanged(long nativeInternalPtr, int state);

  /** Native callback to notify the C++ wrapper that a reward should be given. */
  public static native void grantReward(long nativeInternalPtr, int amount, String rewardType);
}
