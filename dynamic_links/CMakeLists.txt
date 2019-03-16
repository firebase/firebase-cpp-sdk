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

# CMake file for the firebase_dynamic_links library

cmake_minimum_required (VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

include(download_pod_headers)

project(firebase_dynamic_links NONE)
enable_language(C)
enable_language(CXX)

# Common source files used by all platforms
set(common_SRCS
    src/common.cc
    src/common.h
    src/listener.cc)

# Source files used by the Android implementation.
set(android_SRCS
    src/dynamic_links_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/dynamic_links_ios.mm)

# Source files used by the stub implementation.
set(stub_SRCS
    src/dynamic_links_stub.cc)

if(ANDROID)
  set(dynamic_links_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(dynamic_links_platform_SRCS
      "${ios_SRCS}")
else()
  set(dynamic_links_platform_SRCS
      "${stub_SRCS}")
endif()

add_library(firebase_dynamic_links STATIC
    ${common_SRCS}
    ${dynamic_links_platform_SRCS})

# Set up the dependency on Firebase App.
target_link_libraries(firebase_dynamic_links
    firebase_app)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_dynamic_links
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_dynamic_links
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
    TARGET firebase_dynamic_links
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  # Setup the target for getting the Firebase iOS SDK headers through Cocoapods.
  set(pod_target_name "download_dynamic_links_pod_headers")
  set(pods_dir "${PROJECT_BINARY_DIR}/Pods")
  set(pod_list "")
  list(APPEND pod_list "'Firebase/DynamicLinks', '5.18.0'")

  setup_pod_headers_target("${pod_target_name}" "${pods_dir}" "${pod_list}")

  # Add the Cocoapod headers to the include directories
  set(base_header_dir "${pods_dir}/Pods/Headers/Public")
  target_include_directories(firebase_dynamic_links
    PRIVATE
      ${base_header_dir}/FirebaseDynamicLinks
  )

  # Add a dependency to downloading the headers onto dynamic_links.
  add_dependencies(firebase_dynamic_links ${pod_target_name})
endif()
