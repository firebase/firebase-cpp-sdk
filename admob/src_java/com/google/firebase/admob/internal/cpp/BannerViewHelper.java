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
import android.graphics.drawable.ColorDrawable;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewTreeObserver;
import android.widget.PopupWindow;
import com.google.android.gms.ads.AdListener;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.AdView;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Helper class to make interactions between the AdMob C++ wrapper and Java AdView objects cleaner.
 * It's designed to wrap and adapt a single instance of AdView, translate calls coming from C++ into
 * their (typically more complicated) Java equivalents, and convert the Java listener patterns into
 * game engine-friendly state machine polling.
 */
public class BannerViewHelper implements ViewTreeObserver.OnPreDrawListener {

  // It's possible to attempt to show a popup when an activity doesn't have focus. This value
  // controls the number of times the BannerViewHelper object checks for activity window focus
  // before timing out. Assuming 10ms per retry this value attempts to retry for 2 minutes before
  // timing out.
  private static final int POPUP_SHOW_RETRY_COUNT = 12000;

  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Ad Size Types (matches the AdSizeType enumeration in the public C++
  // API). There is only one possible value right now, but this will likely
  // increase.
  public static final int ADSIZETYPE_STANDARD = 0;

  // The number of milliseconds to wait before attempting to create a PopUpWindow to hold an ad.
  private static final int WEBVIEW_DELAY_MILLISECONDS = 200;

  // Pointer to the BannerViewInternalAndroid object that created this object.
  private long mBannerViewInternalPtr;

  // The GMA SDK AdView associated with this helper.
  private AdView mAdView;

  // The current presentation state.
  private int mCurrentPresentationState;

  // Flag indicating whether an ad is showing in mAdView.
  private boolean mAdViewContainsAd;

  // Flag indicating that the Bounding Box listener callback should be invoked
  // the next time mAdView's OnPreDrawListener gets an OnPreDraw event.
  private AtomicBoolean mNotifyListenerOnNextDraw;

  // The {@link Activity} this helper uses to display its {@link AdView}.
  private Activity mActivity;

  // The ad unit ID to use for the {@link AdView}.
  private String mAdUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long mLoadAdCallbackDataPtr;

  // Synchronization object for threadsafing access to mLoadAdCallbackDataPtr.
  private final Object mLoadAdCallbackDataPtrLock;

  // AdSize to use in creating the {@link AdView}.
  private AdSize mAdSize;

  // {@link PopupWindow } that will contain the {@link AdView}. This is done to
  // guarantee the ad is drawn properly even when the application uses a
  // {@link android.app.NativeActivity}.
  private PopupWindow mPopUp;

  // Runnable that is trigged to show the Ad {@link PopupWindow}.
  // When this is set to null, the popup should not be shown.
  private Runnable mPopUpRunnable;

  // Lock used to synchronize the state of the popup window.
  private final Object mPopUpLock;

  // Number of times the BannerViewHelper object has attempted to show the popup window before the
  // activity has focus.
  private int mPopUpShowRetryCount;

  // Flag indicating whether the {@link AdView} is currently intended to be
  // positioned with (x,y) coordinates rather than one of the pre-defined
  // positions (such as ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT).
  private boolean mShouldUseXYForPosition;

  // The user's desired pre-defined position for the {@link AdView}.
  private int mDesiredPosition;

  // The user's desired pre-defined X coordinate for the {@link AdView}.
  private int mDesiredX;

  // The user's desired pre-defined Y coordinate for the {@link AdView}.
  private int mDesiredY;

  /** Constructor. */
  public BannerViewHelper(long bannerViewInternalPtr) {
    mBannerViewInternalPtr = bannerViewInternalPtr;
    mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_HIDDEN;
    mDesiredPosition = ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT;
    mShouldUseXYForPosition = false;
    mAdViewContainsAd = false;
    mNotifyListenerOnNextDraw = new AtomicBoolean(false);
    mLoadAdCallbackDataPtrLock = new Object();
    mPopUpLock = new Object();
    mPopUpShowRetryCount = 0;

    // Test the callbacks and fail quickly if something's wrong.
    completeBannerViewFutureCallback(CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
    notifyStateChanged(CPP_NULLPTR, 0);
  }

  /**
   * Initializes the {@link BannerView}. This creates the corresponding GMA SDK {@link AdView}
   * object and sets it up.
   */
  public void initialize(
      final long callbackDataPtr,
      Activity activity,
      String adUnitID,
      int adSizeType,
      int width,
      int height) {

    // There is only one ad size type right now, which is why that parameter goes unused.
    mAdSize = new AdSize(width, height);
    mActivity = activity;
    mAdUnitId = adUnitID;
    synchronized (mPopUpLock) {
      mPopUpRunnable = null;
    }

    // Create the AdView on the UI thread.
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            int errorCode;
            String errorMessage;
            if (mAdView == null) {
              errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
              mAdView = new AdView(mActivity);
              mAdView.setAdUnitId(mAdUnitId);
              mAdView.setAdSize(mAdSize);
              mAdView.setAdListener(new AdViewListener());
            } else {
              errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
            }

            completeBannerViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
          }
        });
  }

  /** Destroy/deallocate the {@link PopupWindow} and {@link AdView}. */
  public void destroy(final long callbackDataPtr) {
    // If the Activity isn't initialized, there is nothing to destroy.
    if (mActivity == null) {
      return;
    }
    // Stop any attempts to show the popup window.
    synchronized (mPopUpLock) {
      mPopUpRunnable = null;
    }

    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            if (mAdView != null) {
              mAdView.destroy();
              mAdView = null;
            }

            synchronized (mPopUpLock) {
              if (mPopUp != null) {
                mPopUp.dismiss();
                mPopUp = null;
              }
            }

            mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_HIDDEN;

            notifyStateChanged(
                mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
            notifyStateChanged(
                mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_BOUNDING_BOX);
            completeBannerViewFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
            mNotifyListenerOnNextDraw.set(true);
          }
        });
  }

  /** Loads an ad for the underlying AdView object. */
  public void loadAd(long callbackDataPtr, final AdRequest request) {
    if (mActivity == null) {
      return;
    }

    synchronized (mLoadAdCallbackDataPtrLock) {
      if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
        completeBannerViewFutureCallback(
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
            if (mAdView == null) {
              synchronized (mLoadAdCallbackDataPtrLock) {
                completeBannerViewFutureCallback(
                    mLoadAdCallbackDataPtr,
                    ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                    ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
                mLoadAdCallbackDataPtr = CPP_NULLPTR;
              }
            } else {
              mAdView.loadAd(request);
            }
          }
        });
  }

  /** Hides the {@link BannerView}. */
  public void hide(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }

    // Stop any attempts to show the popup window.
    synchronized (mPopUpLock) {
      mPopUpRunnable = null;
    }

    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            int errorCode;
            String errorMessage;

            synchronized (mPopUpLock) {
              if (mAdView == null || mPopUp == null) {
                errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
              } else {
                errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
                mPopUp.dismiss();
                mPopUp = null;
                mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_HIDDEN;
              }
            }

            completeBannerViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
            notifyStateChanged(
                mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
          }
        });
  }

  /** Shows the {@link BannerView}. */
  public void show(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }

    updatePopUpLocation(callbackDataPtr);
  }

  /** Pauses the {@link BannerView}. */
  public void pause(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            if (mAdView != null) {
              mAdView.pause();
            }

            completeBannerViewFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
          }
        });
  }

  /** Resume the {@link BannerView} (from a pause). */
  public void resume(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            if (mAdView != null) {
              mAdView.resume();
            }

            completeBannerViewFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
          }
        });
  }

  /** Moves the {@link BannerView} to the provided (x,y) screen coordinates. */
  public void moveTo(final long callbackDataPtr, int x, int y) {
    if (mActivity == null) {
      return;
    }

    synchronized (mPopUpLock) {
      // Not technically the popup, but these are used as a set, and shouldn't
      // be halfway modified when mPopUpRunnable calls showAtLocation.
      mShouldUseXYForPosition = true;
      mDesiredX = x;
      mDesiredY = y;

      if (mPopUp != null) {
        updatePopUpLocation(callbackDataPtr);
      }
    }
  }

  /** Moves the {@link BannerView} to the provided screen position. */
  public void moveTo(final long callbackDataPtr, final int position) {
    if (mActivity == null) {
      return;
    }

    synchronized (mPopUpLock) {
      // Not technically the popup, but these are used as a set, and shouldn't
      // be halfway modified when mPopUpRunnable calls showAtLocation.
      mShouldUseXYForPosition = false;
      mDesiredPosition = position;

      if (mPopUp != null) {
        updatePopUpLocation(callbackDataPtr);
      }
    }
  }

  /** Returns the current PresentationState of the {@link BannerView}. */
  public int getPresentationState() {
    return mCurrentPresentationState;
  }

  /**
   * Returns an integer array consisting of the current onscreen width, height, x-coordinate, and
   * y-coordinate of the {@link BannerView}. These values make up the BannerView's BoundingBox.
   */
  public int[] getBoundingBox() {
    synchronized (mPopUpLock) {
      int width = -1;
      int height = -1;
      int x = -1;
      int y = -1;
      if (mPopUp != null) {
        int[] onScreenLocation = new int[2];
        mPopUp.getContentView().getLocationOnScreen(onScreenLocation);
        x = onScreenLocation[0];
        y = onScreenLocation[1];

        if (mAdView != null) {
          if (mPopUp.isShowing()) {
            width = mAdView.getWidth();
            height = mAdView.getHeight();
          } else {
            width = height = 0;
          }
        }
      }
      return new int[] {width, height, x, y};
    }
  }

  /**
   * Displays the {@link PopupWindow} that contains the {@link AdView}, in accordance with the
   * parameters of the last call to MoveTo.
   *
   * <p>This method must be called on the UI Thread.
   *
   * @return true if successful, false otherwise.
   */
  private boolean updatePopUpLocation(final long callbackDataPtr) {
    if (mActivity == null) {
      return false;
    }
    final View view = mActivity.findViewById(android.R.id.content);
    if (view == null) {
      return false;
    }

    // If mActivity's content view doesn't have a window token, it will be
    // impossible to update or display the popup later in this method. This is
    // a rare case caused by mActivity spinning up or winding down, but it will
    // cause the WindowManager to crash.
    final View root = view.getRootView();
    if (root == null || root.getWindowToken() == null) {
      return false;
    }

    synchronized (mPopUpLock) {
      if (mPopUp != null) {
        mActivity.runOnUiThread(
            new Runnable() {
              @Override
              public void run() {
                synchronized (mPopUpLock) {
                  // Any change in visibility or position results in the dismissal of the popup (if
                  // one is being displayed) and creation of a fresh one.
                  mPopUp.dismiss();
                  mPopUp = null;
                }
              }
            });
      }

      mPopUpShowRetryCount = 0;
      mPopUpRunnable =
          new Runnable() {
            @Override
            public void run() {
              int errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
              String errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
              // If the Activity's window doesn't currently have focus it's not
              // possible to display the popup window. Poll the focus after a delay of 10ms and try
              // to show the popup again.
              if (!mActivity.hasWindowFocus()) {
                synchronized (mPopUpLock) {
                  if (mPopUpRunnable == null) {
                    errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
                    errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
                  } else {
                    if (mPopUpShowRetryCount < POPUP_SHOW_RETRY_COUNT) {
                      mPopUpShowRetryCount++;
                      new Handler().postDelayed(mPopUpRunnable, 10);
                      return;
                    } else {
                      errorCode = ConstantsHelper.CALLBACK_ERROR_NO_WINDOW_TOKEN;
                      errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NO_WINDOW_TOKEN;
                    }
                  }
                }
              }

              if (mAdView == null) {
                errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
              }

              if (errorCode != ConstantsHelper.CALLBACK_ERROR_NONE) {
                completeBannerViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
                return;
              }

              if (mPopUp == null) {
                mPopUp =
                    new PopupWindow(mAdView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                mPopUp.setBackgroundDrawable(new ColorDrawable(0xFF000000)); // Black
                mAdView.getViewTreeObserver().addOnPreDrawListener(BannerViewHelper.this);

                if (mShouldUseXYForPosition) {
                  mPopUp.showAtLocation(root, Gravity.NO_GRAVITY, mDesiredX, mDesiredY);
                } else {
                  switch (mDesiredPosition) {
                    case ConstantsHelper.AD_VIEW_POSITION_TOP:
                      mPopUp.showAtLocation(root, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_BOTTOM:
                      mPopUp.showAtLocation(root, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT:
                      mPopUp.showAtLocation(root, Gravity.TOP | Gravity.LEFT, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_TOP_RIGHT:
                      mPopUp.showAtLocation(root, Gravity.TOP | Gravity.RIGHT, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_BOTTOM_LEFT:
                      mPopUp.showAtLocation(root, Gravity.BOTTOM | Gravity.LEFT, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_BOTTOM_RIGHT:
                      mPopUp.showAtLocation(root, Gravity.BOTTOM | Gravity.RIGHT, 0, 0);
                      break;
                    default:
                      mPopUp.showAtLocation(root, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 0);
                      break;
                  }
                }
              }

              if (errorCode == ConstantsHelper.CALLBACK_ERROR_NO_WINDOW_TOKEN) {
                mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_HIDDEN;
              } else if (mAdViewContainsAd) {
                mCurrentPresentationState =
                    ConstantsHelper.AD_VIEW_PRESENTATION_STATE_VISIBLE_WITH_AD;
              } else {
                mCurrentPresentationState =
                    ConstantsHelper.AD_VIEW_PRESENTATION_STATE_VISIBLE_WITHOUT_AD;
              }

              completeBannerViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
              notifyStateChanged(
                  mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
              mNotifyListenerOnNextDraw.set(true);
            }
          };
    }

    // TODO(b/31391149): This delay is a workaround for b/31391149, and should be removed once
    // that bug is resolved.
    Handler mainHandler = new Handler(Looper.getMainLooper());
    mainHandler.postDelayed(mPopUpRunnable, WEBVIEW_DELAY_MILLISECONDS);

    return true;
  }

  private class AdViewListener extends AdListener {
    @Override
    public void onAdClosed() {
      mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_VISIBLE_WITH_AD;
      notifyStateChanged(
          mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
      mNotifyListenerOnNextDraw.set(true);
      super.onAdClosed();
    }

    @Override
    public void onAdFailedToLoad(int errorCode) {
      int callbackErrorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
      String callbackErrorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
      switch (errorCode) {
        case AdRequest.ERROR_CODE_INTERNAL_ERROR:
          callbackErrorCode = ConstantsHelper.CALLBACK_ERROR_INTERNAL_ERROR;
          callbackErrorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_INTERNAL_ERROR;
          break;
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
      }

      synchronized (mLoadAdCallbackDataPtrLock) {
        completeBannerViewFutureCallback(
            mLoadAdCallbackDataPtr, callbackErrorCode, callbackErrorMessage);
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }

      super.onAdFailedToLoad(errorCode);
    }

    @Override
    public void onAdLeftApplication() {
      mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_COVERING_UI;
      notifyStateChanged(
          mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
      super.onAdLeftApplication();
    }

    @Override
    public void onAdLoaded() {
      mAdViewContainsAd = true;
      synchronized (mLoadAdCallbackDataPtrLock) {
        completeBannerViewFutureCallback(
            mLoadAdCallbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_NONE,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }
      // Only update the presentation state if the banner view is already visible.
      if (mPopUp != null && mPopUp.isShowing()) {
        mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_VISIBLE_WITH_AD;
        notifyStateChanged(
            mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
        // Loading an ad can sometimes cause the bounds to change.
        mNotifyListenerOnNextDraw.set(true);
      }
      super.onAdLoaded();
    }

    @Override
    public void onAdOpened() {
      mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_COVERING_UI;
      notifyStateChanged(
          mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
      mNotifyListenerOnNextDraw.set(true);
      super.onAdOpened();
    }
  }

  /**
   * Implementation of ViewTreeObserver.OnPreDrawListener's onPreDraw method. This gets called when
   * mAdView is about to be redrawn, and checks a flag before invoking the native callback that
   * tells the C++ side a Bounding Box change has occurred and the BannerView::Listener (if there is
   * one) needs to be notified.
   *
   * <p>By invoking the listener callback here, hooked into the draw loop, the BannerViewHelper
   * object can be sure that any movements of mAdView have been completed and the layout and screen
   * position have been recalculated by the time the notification happens, preventing stale data
   * from getting to the Listener.
   */
  @Override
  public boolean onPreDraw() {
    if (mNotifyListenerOnNextDraw.compareAndSet(true, false)) {
      notifyStateChanged(mBannerViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_BOUNDING_BOX);
    }

    // Returning true tells Android to continue the draw as normal.
    return true;
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeBannerViewFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /** Native callback to notify the C++ wrapper that a state change has occurred. */
  public static native void notifyStateChanged(long nativeInternalPtr, int changeCode);
}
