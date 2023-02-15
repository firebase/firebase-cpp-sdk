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

#ifndef FIREBASE_GMA_SRC_COMMON_AD_VIEW_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_AD_VIEW_INTERNAL_H_

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/ad_view.h"

namespace firebase {
namespace gma {
namespace internal {

// Constants representing each AdView function that returns a Future.
enum AdViewFn {
  kAdViewFnInitialize,
  kAdViewFnLoadAd,
  kAdViewFnHide,
  kAdViewFnShow,
  kAdViewFnPause,
  kAdViewFnResume,
  kAdViewFnDestroy,
  kAdViewFnDestroyOnDelete,
  kAdViewFnSetPosition,
  kAdViewFnCount
};

class AdViewInternal {
 public:
  // Create an instance of whichever subclass of AdViewInternal is
  // appropriate for the current platform.
  static AdViewInternal* CreateInstance(AdView* base);

  // Virtual destructor is required.
  virtual ~AdViewInternal();

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent, const char* ad_unit_id,
                                  const AdSize& size) = 0;

  // Initiates an ad request.
  virtual Future<AdResult> LoadAd(const AdRequest& request) = 0;

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

  // Hides the AdView.
  virtual Future<void> Hide() = 0;

  // Displays the AdView.
  virtual Future<void> Show() = 0;

  // Pauses any background processes associated with the AdView.
  virtual Future<void> Pause() = 0;

  // Resumes from a pause.
  virtual Future<void> Resume() = 0;

  // Cleans up any resources used by this object in preparation for a delete.
  virtual Future<void> Destroy() = 0;

  // Notifies the Bounding Box listener (if one exists) that the AdView's
  // bounding box has changed.
  void NotifyListenerOfBoundingBoxChange(BoundingBox box);

  // Notifies the Ad listener (if one exists) that an event has occurred.
  void NotifyListenerAdClicked();
  void NotifyListenerAdClosed();
  void NotifyListenerAdImpression();
  void NotifyListenerAdOpened();

  // Notifies the Paid Event listener (if one exists) that a paid event has
  // occurred.
  void NotifyListenerOfPaidEvent(const AdValue& ad_value);

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(AdViewFn fn);

  // Retrieves the most recent AdResult future for the LoadAd function.
  Future<AdResult> GetLoadAdLastResult();

  // Returns if the AdView has been initialized.
  virtual bool is_initialized() const = 0;

  // Returns the size of the loaded ad.
  AdSize ad_size() const { return ad_size_; }

 protected:
  friend class firebase::gma::AdView;
  friend class firebase::gma::GmaInternal;

  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit AdViewInternal(AdView* base);

  // Invoked after an AdView has been loaded go reflect the loaded Ad's
  // dimensions. These may differ from the requested dimensions if the AdSize
  // was one of the adaptive size types.
  void update_ad_size_dimensions(int width, int height);

  // A pointer back to the AdView class that created us.
  AdView* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Listener for AdView Lifecycle event callbacks.
  AdListener* ad_listener_;

  // Tracks the size of the AdView.
  AdSize ad_size_;

  // Listener for changes in the AdView's bounding box due to
  // changes in the AdView's position and visibility.
  AdViewBoundingBoxListener* bounding_box_listener_;

  // Listener for any paid events which occur on the AdView.
  PaidEventListener* paid_event_listener_;

  // Lock object for accessing listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_AD_VIEW_INTERNAL_H_
