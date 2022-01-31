# Copyright 2021 Google
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

# CMake file for the firebase_gma library

# Common source files used by all platforms
set(common_SRCS
    src/common/gma_common.cc
    src/common/ad_view.cc
    src/common/ad_view_internal.cc
    src/common/interstitial_ad.cc
    src/common/interstitial_ad_internal.cc
    src/common/full_screen_ad_event_listener.cc
    src/common/rewarded_ad.cc
    src/common/rewarded_ad_internal.cc)

# Define the resource build needed for Android
firebase_cpp_gradle(":gma:gma_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/gma_resources/build/gma_resources_lib.jar")
binary_to_array("gma_resources"
    "${CMAKE_CURRENT_LIST_DIR}/gma_resources/build/gma_resources_lib.jar"
    "firebase_gma"
    "${FIREBASE_GEN_FILE_DIR}/gma")

# Source files used by the Android implementation.
set(android_SRCS
    ${gma_resources_source}
    src/android/ad_request_converter.cc
    src/android/ad_error_android.cc
    src/android/adapter_response_info_android.cc
    src/android/gma_android.cc
    src/android/ad_view_internal_android.cc
    src/android/interstitial_ad_internal_android.cc
    src/android/response_info_android.cc
    src/android/rewarded_ad_internal_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/FADAdSize.mm
    src/ios/FADAdView.mm
    src/ios/FADInterstitialDelegate.mm
    src/ios/FADRequest.mm
    src/ios/FADRewardedAdDelegate.mm
    src/ios/ad_error_ios.mm
    src/ios/adapter_response_info_ios.mm
    src/ios/gma_ios.mm
    src/ios/ad_view_internal_ios.mm
    src/ios/interstitial_ad_internal_ios.mm
    src/ios/response_info_ios.mm
    src/ios/rewarded_ad_internal_ios.mm)

# Source files used by the stub implementation.
set(stub_SRCS
    src/stub/ad_error_stub.cc
    src/stub/adapter_response_info_stub.cc
    src/stub/gma_stub.cc
    src/stub/response_info_stub.cc)

if(ANDROID)
  set(gma_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(gma_platform_SRCS
      "${ios_SRCS}")
else()
  set(gma_platform_SRCS
      "${stub_SRCS}")
endif()

add_library(firebase_gma STATIC
    ${common_SRCS}
    ${gma_platform_SRCS})

set_property(TARGET firebase_gma PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_gma
    PUBLIC firebase_app)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_gma
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_gma
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
  firebase_cpp_proguard_file(gma)
elseif(IOS)
  # GMA for iOS uses weak references, which requires enabling Automatic
  # Reference Counting (ARC). Also enable BitCode.
  target_compile_options(firebase_gma
                         PUBLIC "-fobjc-arc" "-fembed-bitcode")
  target_link_libraries(firebase_gma
                        PUBLIC "-fembed-bitcode")

  setup_pod_headers(
    firebase_gma
    POD_NAMES
      Google-Mobile-Ads-SDK
  )

  # GMA expects the header files to be in a subfolder, so set up a symlink to
  # accomplish that.
  symlink_pod_headers(firebase_gma Google-Mobile-Ads-SDK GoogleMobileAds)

  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_gma PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

cpp_pack_library(firebase_gma "")
cpp_pack_public_headers()
