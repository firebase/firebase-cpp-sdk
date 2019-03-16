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
import com.google.android.gms.ads.NativeExpressAdView;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Helper class to make interactions between the AdMob C++ wrapper and Java NativeExpressAdView
 * objects cleaner. It's designed to wrap and adapt a single instance of NativeExpressAdView,
 * translate calls coming from C++ into their (typically more complicated) Java equivalents, and
 * convert the Java listener patterns into game engine-friendly state machine polling.
 */
public class NativeExpressAdViewHelper implements ViewTreeObserver.OnPreDrawListener {

  // It's possible to attempt to show a popup when an activity doesn't have focus. This value
  // controls the number of times the NativeExpressAdViewHelper object checks for activity window
  // focus before timing out. Assuming 10ms per retry this value attempts to retry for 2 minutes
  // before timing out.
  private static final int POPUP_SHOW_RETRY_COUNT = 12000;

  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Ad Size Types (matches the AdSizeType enumeration in the public C++
  // API). There is only one possible value right now, but this will likely
  // increase.
  public static final int ADSIZETYPE_STANDARD = 0;

  // The number of milliseconds to wait before attempting to create a PopUpWindow to hold an ad.
  private static final int WEBVIEW_DELAY_MILLISECONDS = 200;

  // Pointer to the NativeExpressAdViewInternalAndroid object that created this object.
  private long mNativeExpressAdViewInternalPtr;

  // The GMA SDK NativeExpressAdView associated with this helper.
  private NativeExpressAdView mNativeExpressAdView;

  // The current presentation state.
  private int mCurrentPresentationState;

  // Flag indicating whether an ad is showing in mNativeExpressAdView.
  private boolean mNativeExpressAdViewContainsAd;

  // Flag indicating that the Bounding Box listener callback should be invoked
  // the next time mNativeExpressAdView's OnPreDrawListener gets an OnPreDraw event.
  private AtomicBoolean mNotifyListenerOnNextDraw;

  // The {@link Activity} this helper uses to display its {@link NativeExpressAdView}.
  private Activity mActivity;

  // The ad unit ID to use for the {@link NativeExpressAdView}.
  private String mAdUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long mLoadAdCallbackDataPtr;

  // Synchronization object for threadsafing access to mLoadAdCallbackDataPtr.
  private final Object mLoadAdCallbackDataPtrLock;

  // AdSize to use in creating the {@link AdView}.
  private AdSize mAdSize;

  // {@link PopupWindow } that will contain the {@link NativeExpressAdView}. This is done to
  // guarantee the ad is drawn properly even when the application uses a
  // {@link android.app.NativeActivity}.
  private PopupWindow mPopUp;

  // Runnable that is trigged to show the Ad {@link PopupWindow}.
  // When this is set to null, the popup should not be shown.
  private Runnable mPopUpRunnable;

  // Lock used to synchronize the state of the popup window.
  private final Object mPopUpLock;

  // Number of times the NativeExpressAdViewHelper object has attempted to show the popup window
  // before the activity has focus.
  private int mPopUpShowRetryCount;

  // Flag indicating whether the {@link NativeExpressAdView} is currently intended to be
  // positioned with (x,y) coordinates rather than one of the pre-defined
  // positions (such as ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT).
  private boolean mShouldUseXYForPosition;

  // The user's desired pre-defined position for the {@link NativeExpressAdView}.
  private int mDesiredPosition;

  // The user's desired pre-defined X coordinate for the {@link NativeExpressAdView}.
  private int mDesiredX;

  // The user's desired pre-defined Y coordinate for the {@link NativeExpressAdView}.
  private int mDesiredY;

  /** Constructor. */
  public NativeExpressAdViewHelper(long nativeExpressAdViewInternalPtr) {
    mNativeExpressAdViewInternalPtr = nativeExpressAdViewInternalPtr;
    mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_HIDDEN;
    mDesiredPosition = ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT;
    mShouldUseXYForPosition = false;
    mNativeExpressAdViewContainsAd = false;
    mNotifyListenerOnNextDraw = new AtomicBoolean(false);
    mLoadAdCallbackDataPtrLock = new Object();
    mPopUpLock = new Object();
    mPopUpShowRetryCount = 0;

    // Test the callbacks and fail quickly if something's wrong.
    completeNativeExpressAdViewFutureCallback(
        CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
    notifyStateChanged(CPP_NULLPTR, 0);
  }

  /**
   * Initializes the {@link NativeExpressAdView}. This creates the corresponding GMA SDK {@link
   * NativeExpressAdView} object and sets it up.
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

    // Create the NativeExpressAdView on the UI thread.
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            int errorCode;
            String errorMessage;
            if (mNativeExpressAdView == null) {
              errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
              mNativeExpressAdView = new NativeExpressAdView(mActivity);
              mNativeExpressAdView.setAdUnitId(mAdUnitId);
              mNativeExpressAdView.setAdSize(mAdSize);
              mNativeExpressAdView.setAdListener(new AdViewListener());
            } else {
              errorCode = ConstantsHelper.CALLBACK_ERROR_ALREADY_INITIALIZED;
              errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_ALREADY_INITIALIZED;
            }

            completeNativeExpressAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
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
            if (mNativeExpressAdView != null) {
              mNativeExpressAdView.destroy();
              mNativeExpressAdView = null;
            }

            synchronized (mPopUpLock) {
              if (mPopUp != null) {
                mPopUp.dismiss();
                mPopUp = null;
              }
            }

            mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_HIDDEN;

            notifyStateChanged(
                mNativeExpressAdViewInternalPtr,
                ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
            notifyStateChanged(
                mNativeExpressAdViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_BOUNDING_BOX);
            completeNativeExpressAdViewFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
            mNotifyListenerOnNextDraw.set(true);
          }
        });
  }

  /** Loads an ad for the underlying NativeExpressAdView object. */
  public void loadAd(long callbackDataPtr, final AdRequest request) {
    if (mActivity == null) {
      return;
    }

    synchronized (mLoadAdCallbackDataPtrLock) {
      if (mLoadAdCallbackDataPtr != CPP_NULLPTR) {
        completeNativeExpressAdViewFutureCallback(
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
            if (mNativeExpressAdView == null) {
              synchronized (mLoadAdCallbackDataPtrLock) {
                completeNativeExpressAdViewFutureCallback(
                    mLoadAdCallbackDataPtr,
                    ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
                    ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
                mLoadAdCallbackDataPtr = CPP_NULLPTR;
              }
            } else {
              mNativeExpressAdView.loadAd(request);
            }
          }
        });
  }

  /** Hides the {@link NativeExpressAdView}. */
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
              if (mNativeExpressAdView == null || mPopUp == null) {
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

            completeNativeExpressAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
            notifyStateChanged(
                mNativeExpressAdViewInternalPtr,
                ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
          }
        });
  }

  /** Shows the {@link NativeExpressAdView}. */
  public void show(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }

    updatePopUpLocation(callbackDataPtr);
  }

  /** Pauses the {@link NativeExpressAdView}. */
  public void pause(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            if (mNativeExpressAdView != null) {
              mNativeExpressAdView.pause();
            }

            completeNativeExpressAdViewFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
          }
        });
  }

  /** Resume the {@link NativeExpressAdView} (from a pause). */
  public void resume(final long callbackDataPtr) {
    if (mActivity == null) {
      return;
    }
    mActivity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            if (mNativeExpressAdView != null) {
              mNativeExpressAdView.resume();
            }

            completeNativeExpressAdViewFutureCallback(
                callbackDataPtr,
                ConstantsHelper.CALLBACK_ERROR_NONE,
                ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
          }
        });
  }

  /** Moves the {@link NativeExpressAdView} to the provided (x,y) screen coordinates. */
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

  /** Moves the {@link NativeExpressAdView} to the provided screen position. */
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

  /** Returns the current PresentationState of the {@link NativeExpressAdView}. */
  public int getPresentationState() {
    return mCurrentPresentationState;
  }

  /**
   * Returns an integer array consisting of the current onscreen width, height, x-coordinate, and
   * y-coordinate of the {@link NativeExpressAdView}. These values make up the NativeExpressAdView's
   * BoundingBox.
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

        if (mNativeExpressAdView != null) {
          if (mPopUp.isShowing()) {
            width = mNativeExpressAdView.getWidth();
            height = mNativeExpressAdView.getHeight();
          } else {
            width = height = 0;
          }
        }
      }
      return new int[] {width, height, x, y};
    }
  }

  /**
   * Displays the {@link PopupWindow} that contains the {@link NativeExpressAdView}, in accordance
   * with the parameters of the last call to MoveTo.
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

              if (mNativeExpressAdView == null) {
                errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
              }

              if (errorCode != ConstantsHelper.CALLBACK_ERROR_NONE) {
                completeNativeExpressAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
                return;
              }

              if (mPopUp == null) {
                mPopUp =
                    new PopupWindow(
                        mNativeExpressAdView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                mPopUp.setBackgroundDrawable(new ColorDrawable(0xFF000000)); // Black
                mNativeExpressAdView
                    .getViewTreeObserver()
                    .addOnPreDrawListener(NativeExpressAdViewHelper.this);

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
              } else if (mNativeExpressAdViewContainsAd) {
                mCurrentPresentationState =
                    ConstantsHelper.AD_VIEW_PRESENTATION_STATE_VISIBLE_WITH_AD;
              } else {
                mCurrentPresentationState =
                    ConstantsHelper.AD_VIEW_PRESENTATION_STATE_VISIBLE_WITHOUT_AD;
              }

              completeNativeExpressAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
              notifyStateChanged(
                  mNativeExpressAdViewInternalPtr,
                  ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
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
          mNativeExpressAdViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
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
        completeNativeExpressAdViewFutureCallback(
            mLoadAdCallbackDataPtr, callbackErrorCode, callbackErrorMessage);
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }

      super.onAdFailedToLoad(errorCode);
    }

    @Override
    public void onAdLeftApplication() {
      mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_COVERING_UI;
      notifyStateChanged(
          mNativeExpressAdViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
      super.onAdLeftApplication();
    }

    @Override
    public void onAdLoaded() {
      mNativeExpressAdViewContainsAd = true;
      synchronized (mLoadAdCallbackDataPtrLock) {
        completeNativeExpressAdViewFutureCallback(
            mLoadAdCallbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_NONE,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
        mLoadAdCallbackDataPtr = CPP_NULLPTR;
      }
      // Only update the presentation state if the native express ad view is already visible.
      if (mPopUp != null && mPopUp.isShowing()) {
        mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_VISIBLE_WITH_AD;
        notifyStateChanged(
            mNativeExpressAdViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
        // Loading an ad can sometimes cause the bounds to change.
        mNotifyListenerOnNextDraw.set(true);
      }
      super.onAdLoaded();
    }

    @Override
    public void onAdOpened() {
      mCurrentPresentationState = ConstantsHelper.AD_VIEW_PRESENTATION_STATE_COVERING_UI;
      notifyStateChanged(
          mNativeExpressAdViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_PRESENTATION_STATE);
      mNotifyListenerOnNextDraw.set(true);
      super.onAdOpened();
    }
  }

  /**
   * Implementation of ViewTreeObserver.OnPreDrawListener's onPreDraw method. This gets called when
   * mNativeExpressAdView is about to be redrawn, and checks a flag before invoking the native
   * callback that tells the C++ side a Bounding Box change has occurred and the
   * NativeExpressAdView::Listener (if there is one) needs to be notified.
   *
   * <p>By invoking the listener callback here, hooked into the draw loop, the
   * NativeExpressAdViewHelper object can be sure that any movements of mNativeExpressAdView have
   * been completed and the layout and screen position have been recalculated by the time the
   * notification happens, preventing stale data from getting to the Listener.
   */
  @Override
  public boolean onPreDraw() {
    if (mNotifyListenerOnNextDraw.compareAndSet(true, false)) {
      notifyStateChanged(
          mNativeExpressAdViewInternalPtr, ConstantsHelper.AD_VIEW_CHANGED_BOUNDING_BOX);
    }

    // Returning true tells Android to continue the draw as normal.
    return true;
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeNativeExpressAdViewFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /** Native callback to notify the C++ wrapper that a state change has occurred. */
  public static native void notifyStateChanged(long nativeInternalPtr, int changeCode);
}
