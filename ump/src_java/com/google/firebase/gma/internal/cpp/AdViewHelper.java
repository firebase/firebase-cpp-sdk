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
import android.graphics.drawable.ColorDrawable;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewTreeObserver;
import android.widget.PopupWindow;
import com.google.android.gms.ads.AdListener;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.AdValue;
import com.google.android.gms.ads.AdView;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.OnPaidEventListener;
import com.google.android.gms.ads.ResponseInfo;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Helper class to make interactions between the GMA C++ wrapper and Java AdView objects cleaner.
 * It's designed to wrap and adapt a single instance of AdView, translate calls coming from C++ into
 * their (typically more complicated) Java equivalents, and convert the Java listener patterns into
 * game engine-friendly state machine polling.
 */
public class AdViewHelper implements ViewTreeObserver.OnPreDrawListener {
  // It's possible to attempt to show a popup when an activity doesn't have focus. This value
  // controls the number of times the AdViewHelper object checks for activity window focus
  // before timing out. Assuming 10ms per retry this value attempts to retry for 2 minutes before
  // timing out.
  private static final int POPUP_SHOW_RETRY_COUNT = 12000;

  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // The number of milliseconds to wait before attempting to create a PopUpWindow to hold an ad.
  private static final int WEBVIEW_DELAY_MILLISECONDS = 200;

  // Pointer to the AdViewInternalAndroid object that created this object.
  private long mAdViewInternalPtr;

  // The GMA SDK AdView associated with this helper.
  private AdView mAdView;

  // Flag indicating whether an ad is showing in mAdView.
  private boolean mAdViewContainsAd;

  // Flag indicating that the Bounding Box listener callback should be invoked
  // the next time mAdView's OnPreDrawListener gets an OnPreDraw event.
  private AtomicBoolean mNotifyBoundingBoxListenerOnNextDraw;

  // The {@link Activity} this helper uses to display its {@link AdView}.
  private Activity mActivity;

  // The ad unit ID to use for the {@link AdView}.
  private String mAdUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long mLoadAdCallbackDataPtr;

  // Synchronization object for thread safe access to:
  // * mAdViewInternalPtr
  // * mLoadAdCallbackDataPtr
  private final Object mAdViewLock;

  // {@link PopupWindow } that will contain the {@link AdView}. This is done to
  // guarantee the ad is drawn properly even when the application uses a
  // {@link android.app.NativeActivity}.
  private PopupWindow mPopUp;

  // Runnable that is trigged to show the Ad {@link PopupWindow}.
  // When this is set to null, the popup should not be shown.
  private Runnable mPopUpRunnable;

  // Lock used to synchronize the state of the popup window.
  private final Object mPopUpLock;

  // Number of times the AdViewHelper object has attempted to show the popup window before the
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

  /**
   * Constructor.
   */
  public AdViewHelper(long AdViewInternalPtr, AdView adView) {
    mAdViewInternalPtr = AdViewInternalPtr;
    mAdView = adView;
    mDesiredPosition = ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT;
    mShouldUseXYForPosition = false;
    mAdViewContainsAd = false;
    mNotifyBoundingBoxListenerOnNextDraw = new AtomicBoolean(false);
    mAdViewLock = new Object();
    mPopUpLock = new Object();
    mPopUpShowRetryCount = 0;
  }

  /**
   * Initializes the {@link AdView}. This stores the activity for use with
   * callback and load operations.
   */

  public void initialize(Activity activity) {
    mActivity = activity;
  }

  /**
   * Destroy/deallocate the {@link PopupWindow} and {@link AdView}.
   */
  public void destroy(final long callbackDataPtr, final boolean destructor_invocation) {
    // If the Activity isn't initialized, or already Destroyed, then there's
    // nothing to destroy.
    if (mActivity != null) {
      mActivity.runOnUiThread(new Runnable() {
        @Override
        public void run() {
          // Stop any attempts to show the popup window.
          synchronized (mPopUpLock) {
            mPopUpRunnable = null;
          }

          if (mAdView != null) {
            mAdView.setAdListener(null);
            mAdView.setOnPaidEventListener(null);
            mAdView.destroy();
            mAdView = null;
          }

          synchronized (mPopUpLock) {
            if (mPopUp != null) {
              mPopUp.dismiss();
              mPopUp = null;
            }
          }
          synchronized (mAdViewLock) {
            if (destructor_invocation == false) {
              notifyBoundingBoxChanged(mAdViewInternalPtr);
            }
            mAdViewInternalPtr = CPP_NULLPTR;
          }
          mActivity = null;
          if (destructor_invocation) {
            // AdViews's C++ destructor does not pass a future
            // to callback and complete, but the reference to this object
            // which should be released.
            releaseAdViewGlobalReferenceCallback(callbackDataPtr);
          } else {
            completeAdViewFutureCallback(callbackDataPtr, ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
          }
        }
      });
    } else {
      if (callbackDataPtr != CPP_NULLPTR) {
        completeAdViewFutureCallback(callbackDataPtr, ConstantsHelper.CALLBACK_ERROR_NONE,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
      }
    }
  }

  /**
   * Loads an ad for the underlying AdView object.
   */
  public void loadAd(long callbackDataPtr, final AdRequest request) {
    if (mActivity == null) {
      return;
    }

    synchronized (mAdViewLock) {
      if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
        completeAdViewLoadAdInternalError(callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }
      mLoadAdCallbackDataPtr = callbackDataPtr;
    }

    if (mAdView == null) {
      synchronized (mAdViewLock) {
        completeAdViewLoadAdInternalError(mLoadAdCallbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }
    } else {
      mAdView.loadAd(request);
    }
  }

  /**
   * Hides the {@link AdView}.
   */
  public void hide(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }

    int errorCode;
    String errorMessage;

    synchronized (mPopUpLock) {
      // Stop any attempts to show the popup window.
      mPopUpRunnable = null;

      if (mAdView == null || mPopUp == null) {
        errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
        errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
      } else {
        errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
        errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
        mPopUp.dismiss();
        mPopUp = null;
      }
    }

    completeAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
  }

  /**
   * Shows the {@link AdView}.
   */
  public void show(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }
    updatePopUpLocation(callbackDataPtr);
  }

  /**
   * Pauses the {@link AdView}.
   */
  public void pause(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    } else if (mAdView != null) {
      mAdView.pause();
    }
    completeAdViewFutureCallback(callbackDataPtr, ConstantsHelper.CALLBACK_ERROR_NONE,
        ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  /**
   * Resume the {@link AdView} (from a pause).
   */
  public void resume(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    } else if (mAdView != null) {
      mAdView.resume();
    }

    completeAdViewFutureCallback(callbackDataPtr, ConstantsHelper.CALLBACK_ERROR_NONE,
        ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  /**
   * Moves the {@link AdView} to the provided (x,y) screen coordinates.
   */
  public void moveTo(final long callbackDataPtr, int x, int y) {
    if (mActivity == null) {
      return;
    }

    synchronized (mPopUpLock) {
      mShouldUseXYForPosition = true;
      mDesiredX = x;
      mDesiredY = y;
      if (mPopUp != null) {
        updatePopUpLocation(callbackDataPtr);
      }
    }
  }

  /**
   * Moves the {@link AdView} to the provided screen position.
   */
  public void moveTo(final long callbackDataPtr, final int position) {
    if (mActivity == null) {
      return;
    }

    synchronized (mPopUpLock) {
      mShouldUseXYForPosition = false;
      mDesiredPosition = position;
      if (mPopUp != null) {
        updatePopUpLocation(callbackDataPtr);
      }
    }
  }

  /**
   * Returns an integer array consisting of the current onscreen width, height, x-coordinate, and
   * y-coordinate of the {@link AdView}. These values make up the AdView's BoundingBox.
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
   * Returns an integer representation of the AdView's position.
   */
  public int getPosition() {
    if (mAdView == null || mShouldUseXYForPosition) {
      return ConstantsHelper.AD_VIEW_POSITION_UNDEFINED;
    }
    return mDesiredPosition;
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
        mActivity.runOnUiThread(new Runnable() {
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
      mPopUpRunnable = new Runnable() {
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
                }
                errorCode = ConstantsHelper.CALLBACK_ERROR_NO_WINDOW_TOKEN;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NO_WINDOW_TOKEN;
              }
            }
          }

          if (mAdView == null) {
            errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
            errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
          }

          if (errorCode != ConstantsHelper.CALLBACK_ERROR_NONE) {
            completeAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
            return;
          } else if (mPopUp == null) {
            mPopUp = new PopupWindow(mAdView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            mPopUp.setBackgroundDrawable(new ColorDrawable(0xFF000000)); // Black
            mAdView.getViewTreeObserver().addOnPreDrawListener(AdViewHelper.this);

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

          completeAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
          mNotifyBoundingBoxListenerOnNextDraw.set(true);
        }
      };
    }

    // TODO(b/31391149): This delay is a workaround for b/31391149, and should be removed once
    // that bug is resolved.
    Handler mainHandler = new Handler(Looper.getMainLooper());
    mainHandler.postDelayed(mPopUpRunnable, WEBVIEW_DELAY_MILLISECONDS);

    return true;
  }

  public class AdViewListener extends AdListener implements OnPaidEventListener {
    @Override
    public void onAdClicked() {
      synchronized (mAdViewLock) {
        if (mAdViewInternalPtr != CPP_NULLPTR) {
          notifyAdClicked(mAdViewInternalPtr);
        }
      }
      super.onAdClicked();
    }

    @Override
    public void onAdClosed() {
      synchronized (mAdViewLock) {
        if (mAdViewInternalPtr != CPP_NULLPTR) {
          notifyAdClosed(mAdViewInternalPtr);
          mNotifyBoundingBoxListenerOnNextDraw.set(true);
        }
      }
      super.onAdClosed();
    }

    @Override
    public void onAdFailedToLoad(LoadAdError loadAdError) {
      synchronized (mAdViewLock) {
        if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
          completeAdViewLoadAdError(
              mLoadAdCallbackDataPtr, loadAdError, loadAdError.getCode(), loadAdError.getMessage());
          mLoadAdCallbackDataPtr = CPP_NULLPTR;
        }
      }
      super.onAdFailedToLoad(loadAdError);
    }

    @Override
    public void onAdImpression() {
      synchronized (mAdViewLock) {
        if (mAdViewInternalPtr != CPP_NULLPTR) {
          notifyAdImpression(mAdViewInternalPtr);
        }
      }
      super.onAdImpression();
    }

    @Override
    public void onAdLoaded() {
      synchronized (mAdViewLock) {
        if (mAdView != null) {
          mAdViewContainsAd = true;
        }
        if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
          AdSize adSize = mAdView.getAdSize();
          completeAdViewLoadedAd(mLoadAdCallbackDataPtr, mAdViewInternalPtr, adSize.getWidth(),
              adSize.getHeight(), mAdView.getResponseInfo());
          mLoadAdCallbackDataPtr = CPP_NULLPTR;
        }
        // Only update the bounding box if the banner view is already visible.
        if (mPopUp != null && mPopUp.isShowing()) {
          // Loading an ad can sometimes cause the bounds to change.
          mNotifyBoundingBoxListenerOnNextDraw.set(true);
        }
      }
      super.onAdLoaded();
    }

    @Override
    public void onAdOpened() {
      synchronized (mAdViewLock) {
        if (mAdViewInternalPtr != CPP_NULLPTR) {
          notifyAdOpened(mAdViewInternalPtr);
        }
        mNotifyBoundingBoxListenerOnNextDraw.set(true);
      }
      super.onAdOpened();
    }

    public void onPaidEvent(AdValue value) {
      synchronized (mAdViewLock) {
        if (mAdViewInternalPtr != CPP_NULLPTR) {
          notifyPaidEvent(mAdViewInternalPtr, value.getCurrencyCode(), value.getPrecisionType(),
              value.getValueMicros());
        }
      }
    }
  }

  /**
   * Implementation of ViewTreeObserver.OnPreDrawListener's onPreDraw method. This gets called when
   * mAdView is about to be redrawn, and checks a flag before invoking the native callback that
   * tells the C++ side a Bounding Box change has occurred and the AdView::Listener (if there is
   * one) needs to be notified.
   *
   * <p>By invoking the listener callback here, hooked into the draw loop, the AdViewHelper
   * object can be sure that any movements of mAdView have been completed and the layout and screen
   * position have been recalculated by the time the notification happens, preventing stale data
   * from getting to the Listener.
   */
  @Override
  public boolean onPreDraw() {
    if (mNotifyBoundingBoxListenerOnNextDraw.compareAndSet(true, false)) {
      if (mAdView != null && mAdViewInternalPtr != CPP_NULLPTR) {
        notifyBoundingBoxChanged(mAdViewInternalPtr);
      }
    }
    // Returning true tells Android to continue the draw as normal.
    return true;
  }

  /**
   * Native callback to instruct the C++ wrapper to complete the corresponding future.
   */
  public static native void completeAdViewFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /**
   * Native callback to instruct the C++ wrapper to release its global reference on this
   * object.
   */
  public static native void releaseAdViewGlobalReferenceCallback(long nativeInternalPtr);

  /**
   * Native callback invoked upon successfully loading an ad.
   */
  public static native void completeAdViewLoadedAd(long nativeInternalPtr, long mAdViewInternalPtr,
      int width, int height, ResponseInfo responseInfo);

  /**
   * Native callback upon encountering an error loading an Ad Request. Returns
   * Android Google Mobile Ads SDK error codes.
   **/
  public static native void completeAdViewLoadAdError(
      long nativeInternalPtr, LoadAdError error, int errorCode, String errorMessage);

  /**
   * Native callback upon encountering a wrapper/internal error when
   * processing a Load Ad Request. Returns an integer representing
   * firebase::gma::AdError codes.
   */
  public static native void completeAdViewLoadAdInternalError(
      long nativeInternalPtr, int gmaErrorCode, String errorMessage);

  /**
   * Native callback to notify the C++ wrapper that the Ad's Bounding Box has changed.
   */
  public static native void notifyBoundingBoxChanged(long nativeInternalPtr);

  /**
   * Native callback to notify the C++ wrapper of an ad clicked event
   */
  public static native void notifyAdClicked(long nativeInternalPtr);

  /**
   * Native callback to notify the C++ wrapper of an ad closed event
   */
  public static native void notifyAdClosed(long nativeInternalPtr);

  /**
   * Native callback to notify the C++ wrapper of an ad impression event
   */
  public static native void notifyAdImpression(long nativeInternalPtr);

  /**
   * Native callback to notify the C++ wrapper of an ad opened event
   */
  public static native void notifyAdOpened(long nativeInternalPtr);

  /**
   * Native callback to notify the C++ wrapper that a paid event has occurred.
   */
  public static native void notifyPaidEvent(
      long nativeInternalPtr, String currencyCode, int precisionType, long valueMicros);
}
