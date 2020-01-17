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
firebase_cpp_gradle(":admob:admob_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/admob_resources/build/dexed.jar")
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

if(ANDROID)
  firebase_cpp_proguard_file(admob)
elseif(IOS)
  # AdMob for iOS uses weak references, which requires enabling Automatic
  # Reference Counting (ARC).
  set_property(
    TARGET firebase_admob
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  setup_pod_headers(
    firebase_admob
    POD_NAMES
      Google-Mobile-Ads-SDK
  )

  # AdMob expects the header files to be in a subfolder, so set up a symlink to
  # accomplish that.
  symlink_pod_headers(firebase_admob Google-Mobile-Ads-SDK GoogleMobileAds)
endif()

cpp_pack_library(firebase_admob "")
cpp_pack_public_headers()
