# Copyright 2019 Google
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

# CMake file for the firebase_functions library

cmake_minimum_required (VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

include(binary_to_array)
include(download_pod_headers)

project(firebase_functions NONE)
enable_language(C)
enable_language(CXX)

# Common source files used by all platforms
set(common_SRCS
    src/common/callable_reference.cc
    src/common/callable_result.cc
    src/common/common.cc
    src/common/functions.cc)

# Source files used by the Android implementation.
set(android_SRCS
    src/android/callable_reference_android.cc
    src/android/functions_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/callable_reference_ios.mm
    src/ios/functions_ios.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    src/desktop/callable_reference_desktop.cc
    src/desktop/functions_desktop.cc
    src/desktop/serialization.cc)

if(ANDROID)
  set(functions_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(functions_platform_SRCS
      "${ios_SRCS}")
else()
  set(functions_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS)
  set(additional_link_LIB)
else()
  set(additional_link_LIB
      firebase_rest_lib)
endif()

add_library(firebase_functions STATIC
    ${common_SRCS}
    ${functions_platform_SRCS})

# Set up the dependency on Firebase App.
target_link_libraries(firebase_functions
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_functions
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_functions
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
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_functions
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  # Setup the target for getting the Firebase iOS SDK headers through Cocoapods.
  set(pod_target_name "download_functions_pod_headers")
  set(pods_dir "${PROJECT_BINARY_DIR}/Pods")
  set(pod_list "")
  list(APPEND pod_list "'Firebase/Core', '6.0.0'")
  list(APPEND pod_list "'Firebase/Functions', '6.0.0'")

  setup_pod_headers_target("${pod_target_name}" "${pods_dir}" "${pod_list}")

  # Add the Cocoapod headers to the include directories
  set(base_header_dir "${pods_dir}/Pods/Headers/Public")
  target_include_directories(firebase_functions
    PRIVATE
      ${base_header_dir}/FirebaseCore
      ${base_header_dir}/FirebaseFunctions
  )

  # Add a dependency to downloading the headers onto functions.
  add_dependencies(firebase_functions ${pod_target_name})
endif()
