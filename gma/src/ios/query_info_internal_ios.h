/*
 * Copyright 2024 Google LLC
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

#ifndef FIREBASE_GMA_SRC_IOS_QUERY_INFO_INTERNAL_IOS_H_
#define FIREBASE_GMA_SRC_IOS_QUERY_INFO_INTERNAL_IOS_H_

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>
#endif  // __OBJC__

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#include "app/src/include/firebase/internal/mutex.h"
#include "gma/src/common/query_info_internal.h"

namespace firebase {
namespace gma {
namespace internal {

class QueryInfoInternalIOS : public QueryInfoInternal {
 public:
  explicit QueryInfoInternalIOS(QueryInfo* base);
  ~QueryInfoInternalIOS();

  Future<void> Initialize(AdParent parent) override;
  Future<QueryInfoResult> CreateQueryInfo(AdFormat format,
                                          const AdRequest& request) override;
  Future<QueryInfoResult> CreateQueryInfoWithAdUnit(
      AdFormat format, const AdRequest& request,
      const char* ad_unit_id) override;
  bool is_initialized() const override { return initialized_; }

#ifdef __OBJC__
  void CreateQueryInfoSucceeded(GADQueryInfo *query_info);
  void CreateQueryInfoFailedWithError(NSError *gad_error);
#endif  // __OBJC__

 private:
  /// Prevents duplicate invocations of initialize on the Native Ad.
  bool initialized_;

  /// Contains information to asynchronously complete the createQueryInfo
  /// Future.
  FutureCallbackData<QueryInfoResult>* query_info_callback_data_;

  /// The publisher-provided view (UIView) that's the parent view of the ad.
  /// Declared as an "id" type to avoid referencing an Objective-C class in this
  /// header.
  id parent_view_;

  // Mutex to guard against concurrent operations;
  Mutex mutex_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_IOS_QUERY_INFO_INTERNAL_IOS_H_
