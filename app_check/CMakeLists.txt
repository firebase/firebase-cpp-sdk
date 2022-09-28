# Copyright 2022 Google
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

# CMake file for the firebase_app_check library

# Common source files used by all platforms
set(common_SRCS
  src/common/app_check.cc
  src/common/common.h
  src/include/firebase/app_check.h
  src/include/firebase/app_check/debug_provider.h
  src/include/firebase/app_check/play_integrity_provider.h
  src/include/firebase/app_check/safety_net_provider.h
  src/include/firebase/app_check/device_check_provider.h
  src/include/firebase/app_check/app_attest_provider.h
)

# Source files used by the Android implementation.
set(android_SRCS
  src/android/app_check_android.cc
  src/android/app_check_android.h
  # Supported providers
  src/android/debug_provider.cc
  src/android/play_integrity_provider.cc
  src/android/safety_net_provider.cc
  # Unsupported providers
  src/stub/app_attest_provider.cc
  src/stub/device_check_provider.cc
)

# Source files used by the iOS implementation.
set(ios_SRCS
  src/ios/app_check_ios.mm
  src/ios/app_check_ios.h
  # Supported providers
  src/ios/app_attest_provider.mm
  src/ios/debug_provider.mm
  src/ios/device_check_provider.mm
  # Unsupported providers
  src/stub/play_integrity_provider.cc
  src/stub/safety_net_provider.cc
)

# Source files used by the desktop implementation.
set(desktop_SRCS
  src/desktop/app_check_desktop.cc
  src/desktop/app_check_desktop.h
  # Supported providers
  src/desktop/debug_provider.cc
  # Unsupported providers
  src/stub/app_attest_provider.cc
  src/stub/device_check_provider.cc
  src/stub/play_integrity_provider.cc
  src/stub/safety_net_provider.cc
)

if(ANDROID)
  set(app_check_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(app_check_platform_SRCS
      "${ios_SRCS}")
else()
  set(app_check_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS)
  set(additional_link_LIB)
else()
  set(additional_link_LIB
      firebase_rest_lib)
endif()

add_library(firebase_app_check STATIC
    ${common_SRCS}
    ${app_check_platform_SRCS}
    ${app_check_HDRS})

set_property(TARGET firebase_app_check PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_app_check
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_app_check
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${FIREBASE_CPP_SDK_ROOT_DIR}/ios_pod/swift_headers
)
target_compile_definitions(firebase_app_check
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
  firebase_cpp_proguard_file(app_check)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC) and Bitcode.
  target_compile_options(firebase_app_check
                         PUBLIC "-fobjc-arc" "-fembed-bitcode")
  target_link_libraries(firebase_app_check
                        PUBLIC "-fembed-bitcode")

  setup_pod_headers(
    firebase_app_check
    POD_NAMES
      FirebaseCore
      FirebaseAppCheck
  )

  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_app_check PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

cpp_pack_library(firebase_app_check "")
cpp_pack_public_headers()
