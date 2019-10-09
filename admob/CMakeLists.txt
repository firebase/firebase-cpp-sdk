# Copyright 2018 Google
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# CMake file for the firebase_admob library

cmake_minimum_required (VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

include(download_pod_headers)

project(firebase_admob NONE)
enable_language(C)
enable_language(CXX)

# Common source files used by all platforms
set(common_SRCS
    src/common/admob_common.cc
    src/common/banner_view.cc
    src/common/banner_view_internal.cc
    src/common/interstitial_ad.cc
    src/common/interstitial_ad_internal.cc
    src/common/native_express_ad_view.cc
    src/common/native_express_ad_view_internal.cc
    src/common/rewarded_video.cc
    src/common/rewarded_video_internal.cc)

# Define the resource build needed for Android
binary_to_array("admob_resources"
    "${CMAKE_CURRENT_LIST_DIR}/admob_resources/build/dexed.jar"
    "firebase_admob"
    "${FIREBASE_GEN_FILE_DIR}/admob")

# Source files used by the Android implementation.
set(android_SRCS
    ${admob_resources_source}
    src/android/ad_request_converter.cc
    src/android/admob_android.cc
    src/android/banner_view_internal_android.cc
    src/android/interstitial_ad_internal_android.cc
    src/android/native_express_ad_view_internal_android.cc
    src/android/rewarded_video_internal_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/FADBannerView.mm
    src/ios/FADInterstitialDelegate.mm
    src/ios/FADNativeExpressAdView.mm
    src/ios/FADRequest.mm
    src/ios/FADRewardBasedVideoAdDelegate.mm
    src/ios/admob_ios.mm
    src/ios/banner_view_internal_ios.mm
    src/ios/interstitial_ad_internal_ios.mm
    src/ios/native_express_ad_view_internal_ios.mm
    src/ios/rewarded_video_internal_ios.mm)

# Source files used by the stub implementation.
set(stub_SRCS
    src/stub/admob_stub.cc)

if(ANDROID)
  set(admob_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(admob_platform_SRCS
      "${ios_SRCS}")
else()
  set(admob_platform_SRCS
      "${stub_SRCS}")
endif()

add_library(firebase_admob STATIC
    ${common_SRCS}
    ${admob_platform_SRCS})

set_property(TARGET firebase_admob PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_admob
    firebase_app)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_admob
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_admob
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
)
# Automatically include headers that might not be declared.
if(MSVC)
  add_definitions(/FI"assert.h" /FI"string.h" /FI"stdint.h")
else()
  add_definitions(-include assert.h -include string.h)
endif()

if(IOS)
  # AdMob for iOS uses weak references, which requires enabling Automatic
  # Reference Counting (ARC).
  set_property(
    TARGET firebase_admob
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  # Setup the target for getting the Firebase iOS SDK headers through Cocoapods.
  set(pod_target_name "download_admob_pod_headers")
  set(pods_dir "${PROJECT_BINARY_DIR}/Pods")
  set(pod_list "")
  list(APPEND pod_list "'Firebase/AdMob', '6.9.0'")

  setup_pod_headers_target("${pod_target_name}" "${pods_dir}" "${pod_list}")

  # Add the Cocoapod headers to the include directories
  set(base_header_dir "${pods_dir}/Pods/Headers/Public")
  target_include_directories(firebase_admob
    PRIVATE
      ${base_header_dir}/Google-Mobile-Ads-SDK
  )
  string(CONCAT google_mobile_ads_framework_path
    "${pods_dir}/Pods/Google-Mobile-Ads-SDK/"
    "Frameworks/GoogleMobileAdsFramework-Current/GoogleMobileAds.framework")
  # AdMob expects the header files to be in a subfolder, so set up a symlink to
  # accomplish that.
  symlink_framework_headers(firebase_admob ${pod_target_name}
    ${google_mobile_ads_framework_path} GoogleMobileAds
  )

  # Add a dependency to downloading the headers onto admob.
  add_dependencies(firebase_admob ${pod_target_name})
endif()
