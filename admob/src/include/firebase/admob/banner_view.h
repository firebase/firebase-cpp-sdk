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

#ifndef FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_BANNER_VIEW_H_
#define FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_BANNER_VIEW_H_

#include "firebase/admob/types.h"

#include "firebase/future.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace admob {

namespace internal {
// Forward declaration for platform-specific data, implemented in each library.
class BannerViewInternal;
}  // namespace internal

/// @brief Loads and displays AdMob banner ads.
///
/// Each BannerView object corresponds to a single AdMob banner placement. There
/// are methods to load an ad, move it, show it and hide it, and retrieve the
/// bounds of the ad onscreen.
///
/// BannerView objects maintain a presentation state that indicates whether
/// or not they're currently onscreen, as well as a set of bounds (stored in a
/// @ref BoundingBox struct), but otherwise provide information about
/// their current state through Futures. Methods like @ref Initialize,
/// @ref LoadAd, and @ref Hide each have a corresponding @ref Future from which
/// the result of the last call can be determined. The two variants of
/// @ref SetPosition share a single result @ref Future, since they're
/// essentially the same action.
///
/// In addition, applications can create their own subclasses of
/// @ref BannerView::Listener, pass an instance to the @ref SetListener method,
/// and receive callbacks whenever the presentation state or bounding box of the
/// ad changes.
///
/// For example, you could initialize, load, and show a banner view while
/// checking the result of the previous action at each step as follows:
///
/// @code
/// namespace admob = ::firebase::admob;
/// admob::BannerView* banner_view = new admob::BannerView();
/// banner_view->Initialize(ad_parent, "YOUR_AD_UNIT_ID", desired_ad_size)
/// @endcode
///
/// Then, later:
///
/// @code
/// if (banner_view->InitializeLastResult().status() ==
///     ::firebase::kFutureStatusComplete &&
///     banner_view->InitializeLastResult().error() ==
///     firebase::admob::kAdMobErrorNone) {
///   banner_view->LoadAd(your_ad_request);
/// }
/// @endcode
class BannerView : public AdView {
 public:
  /// Creates an uninitialized @ref BannerView object.
  /// @ref Initialize must be called before the object is used.
  BannerView();

  ~BannerView();

  /// Initializes the @ref BannerView object.
  /// @param[in] parent The platform-specific UI element that will host the ad.
  /// @param[in] ad_unit_id The ad unit ID to use when requesting ads.
  /// @param[in] size The desired ad size for the banner.
  Future<void> Initialize(AdParent parent, const char* ad_unit_id,
                          const AdSize& size);

  /// Returns a @ref Future that has the status of the last call to
  /// @ref Initialize.
  Future<void> InitializeLastResult() const;

  /// Begins an asynchronous request for an ad. If successful, the ad will
  /// automatically be displayed in the BannerView.
  /// @param[in] request An AdRequest struct with information about the request
  ///                    to be made (such as targeting info).
  Future<LoadAdResult> LoadAd(const AdRequest& request);

  /// Returns a @ref Future containing the status of the last call to
  /// @ref LoadAd.
  Future<LoadAdResult> LoadAdLastResult() const;

  /// Retrieves the @ref AdView's current onscreen size and location.
  ///
  /// @return The current size and location. Values are in pixels, and location
  ///         coordinates originate from the top-left corner of the screen.
  BoundingBox bounding_box() const override;

  /// Sets an AdListener for this ad view.
  ///
  /// param[in] listener A listener object which will be invoked when lifecycle
  /// events occur on this AdView.
  void SetAdListener(AdListener *listener) override;

  /// Sets a listener to be invoked when the Ad's bounding box
  /// changes size or location.
  ///
  /// param[in] listener A listener object which will be invoked when the ad
  /// changes size, shape, or position.
  void SetBoundingBoxListener(AdViewBoundingBoxListener *listener) override;

  /// Sets a listener to be invoked when this ad is estimated to have earned
  /// money.
  ///
  /// param[in] A listener object to be invoked when a paid event occurs on the
  /// ad.
  void SetPaidEventListener(PaidEventListener *listener) override;

  /// Moves the @ref BannerView so that its top-left corner is located at
  /// (x, y). Coordinates are in pixels from the top-left corner of the screen.
  ///
  /// When built for Android, the library will not display an ad on top of or
  /// beneath an Activity's status bar. If a call to MoveTo would result in an
  /// overlap, the @ref AdView is placed just below the status bar, so no
  /// overlap occurs.
  /// @param[in] x The desired horizontal coordinate.
  /// @param[in] y The desired vertical coordinate.
  ///
  /// @return a @ref Future which will be completed when this move operation completes.
  Future<void> SetPosition(int x, int y) override;

  /// Moves the @ref AdView so that it's located at the given predefined
  /// position.
  ///
  /// @param[in] position The predefined position to which to move the
  ///   @ref AdView.
  ///
  /// @return a @ref Future which will be completed when this move operation completes.
  Future<void> SetPosition(Position position) override;

  /// Returns a @ref Future containing the status of the last call to either
  /// version of @ref SetPosition.
  Future<void> SetPositionLastResult() const override;

  /// Hides the BannerView.
  Future<void> Hide() override;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Hide.
  Future<void> HideLastResult() const override;

  /// Shows the @ref BannerView.
  Future<void> Show() override;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Show.
  Future<void> ShowLastResult() const override;

  /// Pauses the @ref BannerView. Should be called whenever the C++ engine
  /// pauses or the application loses focus.
  Future<void> Pause() override;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Pause.
  Future<void> PauseLastResult() const override;

  /// Resumes the @ref BannerView after pausing.
  Future<void> Resume() override;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Resume.
  Future<void> ResumeLastResult() const override;

  /// Cleans up and deallocates any resources used by the @ref BannerView.
  Future<void> Destroy() override;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Destroy.
  Future<void> DestroyLastResult() const override;

 private:
  // An internal, platform-specific implementation object that this class uses
  // to interact with the Google Mobile Ads SDKs for iOS and Android.
  internal::BannerViewInternal* internal_;
};

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_BANNER_VIEW_H_
