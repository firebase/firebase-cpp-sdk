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

# CMake file for the firebase_messaging library

cmake_minimum_required (VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

include(binary_to_array)
include(download_pod_headers)

project(firebase_messaging NONE)
enable_language(C)
enable_language(CXX)

# Build the generated header from the flatbuffer schema files.
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
set(flatbuffer_schemas
    ${CMAKE_CURRENT_LIST_DIR}/src/android/schemas/messaging.fbs)
build_flatbuffers("${flatbuffer_schemas}"
                  ""
                  "generate_messaging_fbs"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/messaging"
                  ""
                  "")

# Common source files used by all platforms
set(common_SRCS
    src/common.cc
    src/listener.cc)

# Source files used by the Android implementation.
set(android_SRCS
    ${FIREBASE_GEN_FILE_DIR}/messaging/messaging_generated.h
    src/android/cpp/message_reader.cc
    src/android/cpp/message_reader.h
    src/android/cpp/messaging.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/messaging.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    src/stub/messaging.cc)

if(ANDROID)
  set(messaging_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(messaging_platform_SRCS
      "${ios_SRCS}")
else()
  set(messaging_platform_SRCS
      "${desktop_SRCS}")
endif()

add_library(firebase_messaging STATIC
    ${common_SRCS}
    ${messaging_platform_SRCS})

# Set up the dependency on Firebase App.
target_link_libraries(firebase_messaging
  PUBLIC
    firebase_app
)
if(ANDROID)
  # Android has a dependency on flatbuffers that the other platform do not.
  target_link_libraries(firebase_messaging
    PRIVATE
      flatbuffers
  )
endif()
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_messaging
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
if(ANDROID)
  target_include_directories(firebase_messaging
    PRIVATE
      ${flatbuffers_SOURCE_DIR}/include
  )
endif()
target_compile_definitions(firebase_messaging
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
    TARGET firebase_messaging
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  # Setup the target for getting the Firebase iOS SDK headers through Cocoapods.
  set(pod_target_name "download_messaging_pod_headers")
  set(pods_dir "${PROJECT_BINARY_DIR}/Pods")
  set(pod_list "")
  list(APPEND pod_list "'Firebase/Core', '5.18.0'")
  list(APPEND pod_list "'Firebase/Messaging', '5.18.0'")

  setup_pod_headers_target("${pod_target_name}" "${pods_dir}" "${pod_list}")

  # Add the Cocoapod headers to the include directories
  set(base_header_dir "${pods_dir}/Pods/Headers/Public")
  target_include_directories(firebase_messaging
    PRIVATE
      ${base_header_dir}/FirebaseCore
      ${base_header_dir}/FirebaseMessaging
  )

  # Add a dependency to downloading the headers onto messaging.
  add_dependencies(firebase_messaging ${pod_target_name})
endif()
