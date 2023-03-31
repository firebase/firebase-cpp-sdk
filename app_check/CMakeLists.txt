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
  src/common/debug_provider_common.cc
  src/include/firebase/app_check.h
  src/include/firebase/app_check/debug_provider.h
  src/include/firebase/app_check/play_integrity_provider.h
  src/include/firebase/app_check/device_check_provider.h
  src/include/firebase/app_check/app_attest_provider.h
)

# Run gradle command to make jar
firebase_cpp_gradle(":app_check:app_check_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/app_check_resources/build/app_check_resources_lib.jar")
binary_to_array("app_check_resources"
    "${CMAKE_CURRENT_LIST_DIR}/app_check_resources/build/app_check_resources_lib.jar"
    "firebase_app_check"
    "${FIREBASE_GEN_FILE_DIR}/app_check")

# Source files used by the Android implementation.
set(android_SRCS
  ${app_check_resources_source}
  src/android/app_check_android.cc
  src/android/app_check_android.h
  src/android/common_android.cc
  src/android/common_android.h
  # Supported providers
  src/android/debug_provider_android.cc
  src/android/debug_provider_android.h
  src/android/play_integrity_provider_android.cc
  src/android/play_integrity_provider_android.h
  # Unsupported providers
  src/stub/app_attest_provider_stub.cc
  src/stub/device_check_provider_stub.cc
)

# Source files used by the iOS implementation.
set(ios_SRCS
  src/ios/app_check_ios.mm
  src/ios/app_check_ios.h
  src/ios/util_ios.mm
  src/ios/util_ios.h
  # Supported providers
  src/ios/app_attest_provider_ios.mm
  src/ios/debug_provider_ios.mm
  src/ios/debug_provider_ios.h
  src/ios/device_check_provider_ios.mm
  # Unsupported providers
  src/stub/play_integrity_provider_stub.cc
)

# Flatbuffer resource files used by desktop
binary_to_array("debug_token_request_resource"
    "${CMAKE_CURRENT_LIST_DIR}/src/desktop/debug_token_request.fbs"
    "firebase::app_check"
    "${FIREBASE_GEN_FILE_DIR}/app_check")
binary_to_array("token_response_resource"
    "${CMAKE_CURRENT_LIST_DIR}/src/desktop/token_response.fbs"
    "firebase::app_check"
    "${FIREBASE_GEN_FILE_DIR}/app_check")

# Build the generated header from the flatbuffer schema files.
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
set(flatbuffer_schemas
    ${CMAKE_CURRENT_LIST_DIR}/src/desktop/debug_token_request.fbs
    ${CMAKE_CURRENT_LIST_DIR}/src/desktop/token_response.fbs)
# Because of a bug in the version of Flatbuffers we are pinned to,
# additional flags need to be set.
set(FLATC_ARGS "${FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS}")
build_flatbuffers("${flatbuffer_schemas}"
                  ""
                  "app_check_generate_fbs"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/app_check"
                  ""
                  "")

# Source files used by the desktop implementation.
set(desktop_SRCS
  src/desktop/app_check_desktop.cc
  src/desktop/app_check_desktop.h
  # Supported providers
  src/desktop/debug_provider_desktop.cc
  src/desktop/debug_provider_desktop.h
  src/desktop/debug_token_request.h
  ${debug_token_request_resource_source}
  ${debug_token_request_resource_header}
  ${FIREBASE_GEN_FILE_DIR}/app_check/debug_token_request_generated.h
  src/desktop/token_response.h
  ${token_response_resource_source}
  ${token_response_resource_header}
  ${FIREBASE_GEN_FILE_DIR}/app_check/token_response_generated.h
  # Unsupported providers
  src/stub/app_attest_provider_stub.cc
  src/stub/device_check_provider_stub.cc
  src/stub/play_integrity_provider_stub.cc
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
