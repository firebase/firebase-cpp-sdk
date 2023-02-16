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
  private long adViewInternalPtr;

  // The GMA SDK AdView associated with this helper.
  private AdView adView;

  // Flag indicating whether an ad is showing in adView.
  private boolean adViewContainsAd;

  // Flag indicating that the Bounding Box listener callback should be invoked
  // the next time adView's OnPreDrawListener gets an OnPreDraw event.
  private final AtomicBoolean notifyBoundingBoxListenerOnNextDraw;

  // The {@link Activity} this helper uses to display its {@link AdView}.
  private Activity activity;

  // The ad unit ID to use for the {@link AdView}.
  private String adUnitId;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadAd.
  private long loadAdCallbackDataPtr;

  // Synchronization object for thread safe access to:
  // * adViewInternalPtr
  // * loadAdCallbackDataPtr
  private final Object adViewLock;

  // {@link PopupWindow } that will contain the {@link AdView}. This is done to
  // guarantee the ad is drawn properly even when the application uses a
  // {@link android.app.NativeActivity}.
  private PopupWindow popUp;

  // Runnable that is trigged to show the Ad {@link PopupWindow}.
  // When this is set to null, the popup should not be shown.
  private Runnable popUpRunnable;

  // Lock used to synchronize the state of the popup window.
  private final Object popUpLock;

  // Number of times the AdViewHelper object has attempted to show the popup window before the
  // activity has focus.
  private int popUpShowRetryCount;

  // Flag indicating whether the {@link AdView} is currently intended to be
  // positioned with (x,y) coordinates rather than one of the pre-defined
  // positions (such as ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT).
  private boolean shouldUseXYForPosition;

  // The user's desired pre-defined position for the {@link AdView}.
  private int desiredPosition;

  // The user's desired pre-defined X coordinate for the {@link AdView}.
  private int desiredX;

  // The user's desired pre-defined Y coordinate for the {@link AdView}.
  private int desiredY;

  /** Constructor. */
  public AdViewHelper(long adViewInternalPtr, AdView adView) {
    this.adViewInternalPtr = adViewInternalPtr;
    this.adView = adView;
    desiredPosition = ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT;
    shouldUseXYForPosition = false;
    adViewContainsAd = false;
    notifyBoundingBoxListenerOnNextDraw = new AtomicBoolean(false);
    adViewLock = new Object();
    popUpLock = new Object();
    popUpShowRetryCount = 0;
  }

  /**
   * Initializes the {@link AdView}. This stores the activity for use with callback and load
   * operations.
   */
  public void initialize(Activity activity) {
    this.activity = activity;
  }

  /** Destroy/deallocate the {@link PopupWindow} and {@link AdView}. */
  public void destroy(final long callbackDataPtr, final boolean destructorInvocation) {
    // If the Activity isn't initialized, or already Destroyed, then there's
    // nothing to destroy.
    if (activity != null) {
      activity.runOnUiThread(
          new Runnable() {
            @Override
            public void run() {
              // Stop any attempts to show the popup window.
              synchronized (popUpLock) {
                popUpRunnable = null;
              }

              if (adView != null) {
                adView.setAdListener(null);
                adView.setOnPaidEventListener(null);
                adView.destroy();
                adView = null;
              }

              synchronized (popUpLock) {
                if (popUp != null) {
                  popUp.dismiss();
                  popUp = null;
                }
              }
              synchronized (adViewLock) {
                if (destructorInvocation == false) {
                  notifyBoundingBoxChanged(adViewInternalPtr);
                }
                adViewInternalPtr = CPP_NULLPTR;
              }
              activity = null;
              if (destructorInvocation) {
                // AdViews's C++ destructor does not pass a future
                // to callback and complete, but the reference to this object
                // which should be released.
                releaseAdViewGlobalReferenceCallback(callbackDataPtr);
              } else {
                completeAdViewFutureCallback(
                    callbackDataPtr,
                    ConstantsHelper.CALLBACK_ERROR_NONE,
                    ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
              }
            }
          });
    } else {
      if (callbackDataPtr != CPP_NULLPTR) {
        completeAdViewFutureCallback(
            callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_NONE,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
      }
    }
  }

  /** Loads an ad for the underlying AdView object. */
  public void loadAd(long callbackDataPtr, final AdRequest request) {
    if (activity == null) {
      return;
    }

    synchronized (adViewLock) {
      if (loadAdCallbackDataPtr != CPP_NULLPTR) {
        completeAdViewLoadAdInternalError(
            callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }
      loadAdCallbackDataPtr = callbackDataPtr;
    }

    if (adView == null) {
      synchronized (adViewLock) {
        completeAdViewLoadAdInternalError(
            loadAdCallbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED);
        loadAdCallbackDataPtr = CPP_NULLPTR;
      }
    } else {
      adView.loadAd(request);
    }
  }

  /** Hides the {@link AdView}. */
  public void hide(final long callbackDataPtr) {
    if (activity == null) {
      return;
    }

    int errorCode;
    String errorMessage;

    synchronized (popUpLock) {
      // Stop any attempts to show the popup window.
      popUpRunnable = null;

      if (adView == null || popUp == null) {
        errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
        errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
      } else {
        errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
        errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
        popUp.dismiss();
        popUp = null;
      }
    }

    completeAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
  }

  /** Shows the {@link AdView}. */
  public void show(final long callbackDataPtr) {
    if (activity == null) {
      return;
    }
    var unused = updatePopUpLocation(callbackDataPtr);
  }

  /** Pauses the {@link AdView}. */
  public void pause(final long callbackDataPtr) {
    if (activity == null) {
      return;
    } else if (adView != null) {
      adView.pause();
    }
    completeAdViewFutureCallback(
        callbackDataPtr,
        ConstantsHelper.CALLBACK_ERROR_NONE,
        ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  /** Resume the {@link AdView} (from a pause). */
  public void resume(final long callbackDataPtr) {
    if (activity == null) {
      return;
    } else if (adView != null) {
      adView.resume();
    }

    completeAdViewFutureCallback(
        callbackDataPtr,
        ConstantsHelper.CALLBACK_ERROR_NONE,
        ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  /** Moves the {@link AdView} to the provided (x,y) screen coordinates. */
  public void moveTo(final long callbackDataPtr, int x, int y) {
    if (activity == null) {
      return;
    }

    synchronized (popUpLock) {
      shouldUseXYForPosition = true;
      desiredX = x;
      desiredY = y;
      if (popUp != null) {
        var unused = updatePopUpLocation(callbackDataPtr);
      }
    }
  }

  /** Moves the {@link AdView} to the provided screen position. */
  public void moveTo(final long callbackDataPtr, final int position) {
    if (activity == null) {
      return;
    }

    synchronized (popUpLock) {
      shouldUseXYForPosition = false;
      desiredPosition = position;
      if (popUp != null) {
        var unused = updatePopUpLocation(callbackDataPtr);
      }
    }
  }

  /**
   * Returns an integer array consisting of the current onscreen width, height, x-coordinate, and
   * y-coordinate of the {@link AdView}. These values make up the AdView's BoundingBox.
   */
  public int[] getBoundingBox() {
    synchronized (popUpLock) {
      int width = -1;
      int height = -1;
      int x = -1;
      int y = -1;
      if (popUp != null) {
        int[] onScreenLocation = new int[2];
        popUp.getContentView().getLocationOnScreen(onScreenLocation);
        x = onScreenLocation[0];
        y = onScreenLocation[1];

        if (adView != null) {
          if (popUp.isShowing()) {
            width = adView.getWidth();
            height = adView.getHeight();
          } else {
            width = height = 0;
          }
        }
      }
      return new int[] {width, height, x, y};
    }
  }

  /** Returns an integer representation of the AdView's position. */
  public int getPosition() {
    if (adView == null || shouldUseXYForPosition) {
      return ConstantsHelper.AD_VIEW_POSITION_UNDEFINED;
    }
    return desiredPosition;
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
    if (activity == null) {
      return false;
    }
    final View view = activity.findViewById(android.R.id.content);
    if (view == null) {
      return false;
    }

    // If activity's content view doesn't have a window token, it will be
    // impossible to update or display the popup later in this method. This is
    // a rare case caused by activity spinning up or winding down, but it will
    // cause the WindowManager to crash.
    final View root = view.getRootView();
    if (root == null || root.getWindowToken() == null) {
      return false;
    }

    synchronized (popUpLock) {
      if (popUp != null) {
        activity.runOnUiThread(
            new Runnable() {
              @Override
              public void run() {
                synchronized (popUpLock) {
                  // Any change in visibility or position results in the dismissal of the popup (if
                  // one is being displayed) and creation of a fresh one.
                  popUp.dismiss();
                  popUp = null;
                }
              }
            });
      }

      popUpShowRetryCount = 0;
      popUpRunnable =
          new Runnable() {
            @Override
            public void run() {
              int errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;
              String errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE;
              // If the Activity's window doesn't currently have focus it's not
              // possible to display the popup window. Poll the focus after a delay of 10ms and try
              // to show the popup again.
              if (!activity.hasWindowFocus()) {
                synchronized (popUpLock) {
                  if (popUpRunnable == null) {
                    errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
                    errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
                  } else {
                    if (popUpShowRetryCount < POPUP_SHOW_RETRY_COUNT) {
                      popUpShowRetryCount++;
                      new Handler().postDelayed(popUpRunnable, 10);
                      return;
                    }
                    errorCode = ConstantsHelper.CALLBACK_ERROR_NO_WINDOW_TOKEN;
                    errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_NO_WINDOW_TOKEN;
                  }
                }
              }

              if (adView == null) {
                errorCode = ConstantsHelper.CALLBACK_ERROR_UNINITIALIZED;
                errorMessage = ConstantsHelper.CALLBACK_ERROR_MESSAGE_UNINITIALIZED;
              }

              if (errorCode != ConstantsHelper.CALLBACK_ERROR_NONE) {
                completeAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
                return;
              } else if (popUp == null) {
                popUp =
                    new PopupWindow(adView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                popUp.setBackgroundDrawable(new ColorDrawable(0xFF000000)); // Black
                adView.getViewTreeObserver().addOnPreDrawListener(AdViewHelper.this);

                if (shouldUseXYForPosition) {
                  popUp.showAtLocation(root, Gravity.NO_GRAVITY, desiredX, desiredY);
                } else {
                  switch (desiredPosition) {
                    case ConstantsHelper.AD_VIEW_POSITION_TOP:
                      popUp.showAtLocation(root, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_BOTTOM:
                      popUp.showAtLocation(root, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_TOP_LEFT:
                      popUp.showAtLocation(root, Gravity.TOP | Gravity.LEFT, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_TOP_RIGHT:
                      popUp.showAtLocation(root, Gravity.TOP | Gravity.RIGHT, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_BOTTOM_LEFT:
                      popUp.showAtLocation(root, Gravity.BOTTOM | Gravity.LEFT, 0, 0);
                      break;
                    case ConstantsHelper.AD_VIEW_POSITION_BOTTOM_RIGHT:
                      popUp.showAtLocation(root, Gravity.BOTTOM | Gravity.RIGHT, 0, 0);
                      break;
                    default:
                      popUp.showAtLocation(root, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 0);
                      break;
                  }
                }
              }

              completeAdViewFutureCallback(callbackDataPtr, errorCode, errorMessage);
              notifyBoundingBoxListenerOnNextDraw.set(true);
            }
          };
    }

    // TODO(b/31391149): This delay is a workaround for b/31391149, and should be removed once
    // that bug is resolved.
    Handler mainHandler = new Handler(Looper.getMainLooper());
    mainHandler.postDelayed(popUpRunnable, WEBVIEW_DELAY_MILLISECONDS);

    return true;
  }

  /** A listener for receiving notifications during the lifecycle of a BannerAd. */
  public class AdViewListener extends AdListener implements OnPaidEventListener {
    @Override
    public void onAdClicked() {
      synchronized (adViewLock) {
        if (adViewInternalPtr != CPP_NULLPTR) {
          notifyAdClicked(adViewInternalPtr);
        }
      }
      super.onAdClicked();
    }

    @Override
    public void onAdClosed() {
      synchronized (adViewLock) {
        if (adViewInternalPtr != CPP_NULLPTR) {
          notifyAdClosed(adViewInternalPtr);
          notifyBoundingBoxListenerOnNextDraw.set(true);
        }
      }
      super.onAdClosed();
    }

    @Override
    public void onAdFailedToLoad(LoadAdError loadAdError) {
      synchronized (adViewLock) {
        if (loadAdCallbackDataPtr != CPP_NULLPTR) {
          completeAdViewLoadAdError(
              loadAdCallbackDataPtr, loadAdError, loadAdError.getCode(), loadAdError.getMessage());
          loadAdCallbackDataPtr = CPP_NULLPTR;
        }
      }
      super.onAdFailedToLoad(loadAdError);
    }

    @Override
    public void onAdImpression() {
      synchronized (adViewLock) {
        if (adViewInternalPtr != CPP_NULLPTR) {
          notifyAdImpression(adViewInternalPtr);
        }
      }
      super.onAdImpression();
    }

    @Override
    public void onAdLoaded() {
      synchronized (adViewLock) {
        if (adView != null) {
          adViewContainsAd = true;
        }
        if (loadAdCallbackDataPtr != CPP_NULLPTR) {
          AdSize adSize = adView.getAdSize();
          completeAdViewLoadedAd(
              loadAdCallbackDataPtr,
              adViewInternalPtr,
              adSize.getWidth(),
              adSize.getHeight(),
              adView.getResponseInfo());
          loadAdCallbackDataPtr = CPP_NULLPTR;
        }
        // Only update the bounding box if the banner view is already visible.
        if (popUp != null && popUp.isShowing()) {
          // Loading an ad can sometimes cause the bounds to change.
          notifyBoundingBoxListenerOnNextDraw.set(true);
        }
      }
      super.onAdLoaded();
    }

    @Override
    public void onAdOpened() {
      synchronized (adViewLock) {
        if (adViewInternalPtr != CPP_NULLPTR) {
          notifyAdOpened(adViewInternalPtr);
        }
        notifyBoundingBoxListenerOnNextDraw.set(true);
      }
      super.onAdOpened();
    }

    @Override
    public void onPaidEvent(AdValue value) {
      synchronized (adViewLock) {
        if (adViewInternalPtr != CPP_NULLPTR) {
          notifyPaidEvent(
              adViewInternalPtr,
              value.getCurrencyCode(),
              value.getPrecisionType(),
              value.getValueMicros());
        }
      }
    }
  }

  /**
   * Implementation of ViewTreeObserver.OnPreDrawListener's onPreDraw method. This gets called when
   * adView is about to be redrawn, and checks a flag before invoking the native callback that tells
   * the C++ side a Bounding Box change has occurred and the AdView::Listener (if there is one)
   * needs to be notified.
   *
   * <p>By invoking the listener callback here, hooked into the draw loop, the AdViewHelper object
   * can be sure that any movements of adView have been completed and the layout and screen position
   * have been recalculated by the time the notification happens, preventing stale data from getting
   * to the Listener.
   */
  @Override
  public boolean onPreDraw() {
    if (notifyBoundingBoxListenerOnNextDraw.compareAndSet(true, false)) {
      if (adView != null && adViewInternalPtr != CPP_NULLPTR) {
        notifyBoundingBoxChanged(adViewInternalPtr);
      }
    }
    // Returning true tells Android to continue the draw as normal.
    return true;
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeAdViewFutureCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  /** Native callback to instruct the C++ wrapper to release its global reference on this object. */
  public static native void releaseAdViewGlobalReferenceCallback(long nativeInternalPtr);

  /** Native callback invoked upon successfully loading an ad. */
  public static native void completeAdViewLoadedAd(
      long nativeInternalPtr,
      long mAdViewInternalPtr,
      int width,
      int height,
      ResponseInfo responseInfo);

  /**
   * Native callback upon encountering an error loading an Ad Request. Returns Android Google Mobile
   * Ads SDK error codes.
   */
  public static native void completeAdViewLoadAdError(
      long nativeInternalPtr, LoadAdError error, int errorCode, String errorMessage);

  /**
   * Native callback upon encountering a wrapper/internal error when processing a Load Ad Request.
   * Returns an integer representing firebase::gma::AdError codes.
   */
  public static native void completeAdViewLoadAdInternalError(
      long nativeInternalPtr, int gmaErrorCode, String errorMessage);

  /** Native callback to notify the C++ wrapper that the Ad's Bounding Box has changed. */
  public static native void notifyBoundingBoxChanged(long nativeInternalPtr);

  /** Native callback to notify the C++ wrapper of an ad clicked event */
  public static native void notifyAdClicked(long nativeInternalPtr);

  /** Native callback to notify the C++ wrapper of an ad closed event */
  public static native void notifyAdClosed(long nativeInternalPtr);

  /** Native callback to notify the C++ wrapper of an ad impression event */
  public static native void notifyAdImpression(long nativeInternalPtr);

  /** Native callback to notify the C++ wrapper of an ad opened event */
  public static native void notifyAdOpened(long nativeInternalPtr);

  /** Native callback to notify the C++ wrapper that a paid event has occurred. */
  public static native void notifyPaidEvent(
      long nativeInternalPtr, String currencyCode, int precisionType, long valueMicros);
}
