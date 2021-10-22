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

#ifndef FIREBASE_ADMOB_SRC_COMMON_BANNER_VIEW_INTERNAL_H_
#define FIREBASE_ADMOB_SRC_COMMON_BANNER_VIEW_INTERNAL_H_

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/banner_view.h"
#include "app/src/include/firebase/future.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {
namespace internal {

// Constants representing each BannerView function that returns a Future.
enum BannerViewFn {
  kBannerViewFnInitialize,
  kBannerViewFnLoadAd,
  kBannerViewFnHide,
  kBannerViewFnShow,
  kBannerViewFnPause,
  kBannerViewFnResume,
  kBannerViewFnDestroy,
  kBannerViewFnDestroyOnDelete,
  kBannerViewFnSetPosition,
  kBannerViewFnCount
};

class BannerViewInternal {
 public:
  // Create an instance of whichever subclass of BannerViewInternal is
  // appropriate for the current platform.
  static BannerViewInternal* CreateInstance(BannerView* base);

  // Virtual destructor is required.
  virtual ~BannerViewInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent, const char* ad_unit_id,
                                  const AdSize& size) = 0;

  // Initiates an ad request.
  virtual Future<LoadAdResult> LoadAd(const AdRequest& request) = 0;

  // Retrieves the @ref AdView's current onscreen size and location.
  virtual BoundingBox bounding_box() const = 0;

  // Sets an AdListener for this ad view.
  virtual void SetAdListener(AdListener* listener);

  // Sets a listener to be invoked when the Ad's bounding box
  // changes size or location.
  virtual void SetBoundingBoxListener(AdViewBoundingBoxListener* listener);

  // Sets a listener to be invoked when this ad is estimated to have earned
  // money.
  virtual void SetPaidEventListener(PaidEventListener* listener);

  /// Moves the @ref AdView so that its top-left corner is located at
  /// (x, y). Coordinates are in pixels from the top-left corner of the screen.
  ///
  virtual Future<void> SetPosition(int x, int y) = 0;

  /// Moves the @ref AdView so that it's located at the given predefined
  /// position.
  virtual Future<void> SetPosition(AdView::Position position) = 0;

  // Hides the banner view.
  virtual Future<void> Hide() = 0;

  // Displays a banner view.
  virtual Future<void> Show() = 0;

  // Pauses any background processes associated with the banner view.
  virtual Future<void> Pause() = 0;

  // Resumes from a pause.
  virtual Future<void> Resume() = 0;

  // Cleans up any resources used by this object in preparation for a delete.
  virtual Future<void> Destroy() = 0;

  // Notifies the Bounding Box listener (if one exists) that the banner view's
  // bounding box has changed.
  void NotifyListenerOfBoundingBoxChange(BoundingBox box);

  // Notifies the Ad listener (if one exists) that an even has occurred.
  void NotifyListenerAdClicked();
  void NotifyListenerAdClosed();
  void NotifyListenerAdImpression();
  void NotifyListenerAdOpened();

  // Notifies the Paid Event listener (if one exists) that a paid event has
  // occurred.
  void NotifyListenerOfPaidEvent(const AdValue& ad_value);

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(BannerViewFn fn);

  // Retrieves the most recent LoadAdResult future for the LoadAd function.
  Future<LoadAdResult> GetLoadAdLastResult();

 protected:
  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit BannerViewInternal(BannerView* base);

  // A pointer back to the BannerView class that created us.
  BannerView* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  AdListener* ad_listener_;
  AdViewBoundingBoxListener* bounding_box_listener_;
  PaidEventListener* paid_event_listener_;

  // Lock object for accessing listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_COMMON_BANNER_VIEW_INTERNAL_H_
