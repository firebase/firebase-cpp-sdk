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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_BANNER_VIEW_INTERNAL_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_BANNER_VIEW_INTERNAL_H_

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
  kBannerViewFnMoveTo,
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
                                  AdSize size) = 0;

  // Initiates an ad request.
  virtual Future<void> LoadAd(const AdRequest& request) = 0;

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

  // Moves the banner view so that its top-left corner is located at (x, y).
  virtual Future<void> MoveTo(int x, int y) = 0;

  // Moves the banner view so that it's located at the given pre-defined
  // position.
  virtual Future<void> MoveTo(BannerView::Position position) = 0;

  // Returns the current presentation state of the banner view.
  virtual BannerView::PresentationState GetPresentationState() const = 0;

  // Retrieves the banner view's current on-screen size and location.
  virtual BoundingBox GetBoundingBox() const = 0;

  // Sets the listener that should be informed of presentation state and
  // bounding box changes.
  void SetListener(BannerView::Listener* listener);

  // Notifies the listener (if one exists) that the presentation state has
  // changed.
  void NotifyListenerOfPresentationStateChange(
      BannerView::PresentationState state);

  // Notifies the listener (if one exists) that the banner view's bounding box
  // has changed.
  void NotifyListenerOfBoundingBoxChange(BoundingBox box);

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(BannerViewFn fn);

 protected:
  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit BannerViewInternal(BannerView* base);

  // A pointer back to the BannerView class that created us.
  BannerView* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Reference to the listener to which this object sends callbacks.
  BannerView::Listener* listener_;

  // Lock object for accessing listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_BANNER_VIEW_INTERNAL_H_
