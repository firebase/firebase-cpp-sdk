# Copyright 2026 Google LLC
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

# CMake file for the cross-platform native streaming HTTP transport library
# (`firebase_http_transport`).

cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(http_transport_SRCS
    http_bridge.h
    http_transport.h
    http_transport_common.cc)

if(ANDROID)
  list(APPEND http_transport_SRCS http_transport_android.cc)
elseif(IOS)
  list(APPEND http_transport_SRCS http_transport_apple.mm)
else()
  list(APPEND http_transport_SRCS http_transport_curl.cc)
endif()

add_library(firebase_http_transport STATIC
    ${http_transport_SRCS})

set_property(TARGET firebase_http_transport PROPERTY FOLDER "Firebase Cpp")

# Match firebase_app's -fno-exceptions / /EHs-c- build settings.
if(MSVC)
  target_compile_options(firebase_http_transport PRIVATE /EHs-c-)
else()
  target_compile_options(firebase_http_transport PRIVATE -fno-exceptions)
endif()

target_include_directories(firebase_http_transport
  PUBLIC
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)

target_link_libraries(firebase_http_transport
  PUBLIC
    firebase_app
)

if(ANDROID)
  # Android uses JNI HttpURLConnection directly via firebase_app's JNI helpers.
elseif(IOS)
  set_source_files_properties(http_transport_apple.mm
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
  )
  target_link_libraries(firebase_http_transport
    PRIVATE
      "-framework Foundation"
  )
else()
  target_include_directories(firebase_http_transport
    PRIVATE
      ${CURL_SOURCE_DIR}/include
  )
  if(MSVC)
    target_compile_definitions(firebase_http_transport
      PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        CURL_STATICLIB=1
    )
  endif()
  target_link_libraries(firebase_http_transport
    PRIVATE
      firebase_rest_lib
      libcurl
  )
endif()

if(NOT ANDROID AND NOT IOS)
  cpp_pack_library(firebase_http_transport "deps/app")
  if(FIREBASE_CPP_BUILD_TESTS)
    add_subdirectory(tests)
  endif()
endif()
