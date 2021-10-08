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

# CMake file for the firebase_performance library

# Common source files used by all platforms
set(common_SRCS
    src/performance_common.cc)

# Source files used by the Android implementation.
set(android_SRCS
    ${performance_resources_source}
    src/android/firebase_performance.cc
    src/android/http_metric.cc
    src/android/trace.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/firebase_performance.mm
    src/ios/http_metric.mm
    src/ios/trace.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    src/stubs/http_metric_stub.cc
    src/stubs/performance_stub.cc
    src/stubs/trace_stub.cc)

if(ANDROID)
  set(performance_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(performance_platform_SRCS
      "${ios_SRCS}")
else()
  set(performance_platform_SRCS
        "${desktop_SRCS}")
endif()

if(ANDROID OR IOS)
  set(additional_include_DIR)
  set(additional_link_LIB)
  set(additional_DEFINES)
else()
  set(additional_include_DIR
      ${OPENSSL_INCLUDE_DIR})

  set(additional_link_LIB
      firebase_rest_lib
      OpenSSL::SSL
      OpenSSL::Crypto)

  set(additional_DEFINES
      -DFIREBASE_TARGET_DESKTOP=1)
endif()

if(MSVC)
  set(additional_DEFINES
    "${additional_DEFINES}"
    -DWIN32_LEAN_AND_MEAN # Ensure that windows doesn't include winsock.h by
                            # default, as it can cause issues when libraries try
                            # to include winsock2.h later on in the process.
  )
endif()

add_library(firebase_performance STATIC
    ${common_SRCS}
    ${performance_platform_SRCS})

set_property(TARGET firebase_performance PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_performance
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_performance
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${additional_include_DIR}
)
target_compile_definitions(firebase_performance
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
    ${additional_DEFINES}
)
# Automatically include headers that might not be declared.
if(MSVC)
  add_definitions(/FI"assert.h" /FI"string.h" /FI"stdint.h")
else()
  add_definitions(-include assert.h -include string.h)
endif()

if(ANDROID)
  firebase_cpp_proguard_file(database)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC) and Bitcode.
  target_compile_options(firebase_performance
                         PUBLIC "-fobjc-arc" "-fembed-bitcode")
  target_link_libraries(firebase_performance
                        PUBLIC "-fembed-bitcode")

  setup_pod_headers(
    firebase_performance
    POD_NAMES
      FirebaseCore
      FirebasePerformance
  )
  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_performance PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_performance "")
cpp_pack_public_headers()
if (NOT ANDROID AND NOT IOS)
  cpp_pack_library(uv_a "deps/performance/external")
  cpp_pack_library(libuWS "deps/performance/external")
endif()
