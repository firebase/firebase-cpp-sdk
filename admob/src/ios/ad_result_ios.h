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

#ifndef FIREBASE_ADMOB_SRC_IOS_AD_RESULT_IOS_H_
#define FIREBASE_ADMOB_SRC_IOS_AD_RESULT_IOS_H_

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

#include <string>

#include "admob/src/include/firebase/admob/types.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {

struct AdResultInternal {
  // True if the result contains an error originating from C++/Java wrapper
  // code. If false, then an Admob Android AdError has occurred.
  bool is_wrapper_error;

  // True if this was a successful result.
  bool is_successful;

  // An error code
  AdMobError code;

  // A cached value of com.google.android.gms.ads.AdError.domain
  std::string domain;

  // A cached value of com.google.android.gms.ads.AdError.message
  std::string message;

  // A cached result from invoking com.google.android.gms.ads.AdError.ToString.
  std::string to_string;

  // If this is not a successful result, or if it's a wrapper error, then
  // ios_error is a pointer to an NSError produced by the Admob iOS SDK.
  const NSError* ios_error;

  Mutex mutex;
};

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_IOS_AD_RESULT_IOS_H_
