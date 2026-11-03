# Copyright 2021 Google LLC
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

set(common_SRCS
    src/performance_common.cc
)

set(android_SRCS
    src/android/firebase_performance.cc
    src/android/http_metric.cc
    src/android/trace.cc
)

set(ios_SRCS
    src/ios/firebase_performance.mm
    src/ios/http_metric.mm
    src/ios/trace.mm
)

set(stub_SRCS
    stubs/performance_stub.cc
    stubs/http_metric_stub.cc
    stubs/trace_stub.cc
)

if(ANDROID)
  set(performance_platform_SRCS "${android_SRCS}")
elseif(IOS)
  set(performance_platform_SRCS "${ios_SRCS}")
else()
  set(performance_platform_SRCS "${stub_SRCS}")
endif()

add_library(firebase_performance STATIC
    ${common_SRCS}
    ${performance_platform_SRCS})

set_property(TARGET firebase_performance PROPERTY FOLDER "Firebase Cpp")

target_link_libraries(firebase_performance
    PUBLIC firebase_app)

target_include_directories(firebase_performance
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${FIREBASE_GEN_FILE_DIR}
    ${FIREBASE_GEN_FILE_DIR}/app/src/include
)

target_compile_definitions(firebase_performance
  PUBLIC
    INTERNAL_EXPERIMENTAL=1
)

if(IOS)
  # Enable Automatic Reference Counting (ARC) for Objective-C/Objective-C++ sources.
  target_compile_options(firebase_performance
                         PRIVATE $<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc>)

  setup_spm_headers(
    firebase_performance
    MODULES
      FirebaseCore
      FirebasePerformance
  )

  if(FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_performance PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

if(FIREBASE_CPP_BUILD_STUB_TESTS)
  # Runs the unit test suite against the desktop stub implementation (where
  # Android/iOS Reporter expectations are no-ops).
  firebase_cpp_cc_test(
    firebase_performance_test
    SOURCES
      src/performance_test.cc
      src/http_metric_test.cc
      src/trace_test.cc
    DEPENDS
      firebase_app_for_testing
      firebase_performance
      firebase_testing
  )
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_performance "")
cpp_pack_public_headers()
