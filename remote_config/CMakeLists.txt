# Copyright 2019 Google LLC
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

# CMake file for the firebase_remote_config library

binary_to_array("request_resource"
    "${CMAKE_CURRENT_LIST_DIR}/src/desktop/request.fbs"
    "firebase::remote_config"
    "${FIREBASE_GEN_FILE_DIR}/remote_config")
binary_to_array("response_resource"
    "${CMAKE_CURRENT_LIST_DIR}/src/desktop/response.fbs"
    "firebase::remote_config"
    "${FIREBASE_GEN_FILE_DIR}/remote_config")

# Build the generated header from the flatbuffer schema files.
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
set(flatbuffer_schemas
    ${CMAKE_CURRENT_LIST_DIR}/src/desktop/request.fbs
    ${CMAKE_CURRENT_LIST_DIR}/src/desktop/response.fbs)
# Because of a bug in the version of Flatbuffers we are pinned to,
# additional flags need to be set.
set(FLATC_ARGS "${FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS}")
build_flatbuffers("${flatbuffer_schemas}"
                  ""
                  "remote_config_generate_request_fbs"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/remote_config"
                  ""
                  "")

# Common source files used by all platforms
set(common_SRCS
    src/common.cc
    src/cleanup.h
    src/config_update_listener_registration.cc
    src/config_update_listener_registration_internal.cc
    src/remote_config.cc)


# Run gradle command to make jar
firebase_cpp_gradle(":remote_config:remote_config_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/remote_config_resources/build/remote_config_resources_lib.jar")
binary_to_array("remote_config_resources"
    "${CMAKE_CURRENT_LIST_DIR}/remote_config_resources/build/remote_config_resources_lib.jar"
    "firebase_remote_config"
    "${FIREBASE_GEN_FILE_DIR}/remote_config")

# Source files used by the Android implementation.
set(android_SRCS
    ${remote_config_resources_source}
    src/android/remote_config_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/remote_config_ios.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    ${request_resource_source}
    ${request_resource_header}
    ${response_resource_source}
    ${response_resource_header}
    ${FIREBASE_GEN_FILE_DIR}/remote_config/request_generated.h
    ${FIREBASE_GEN_FILE_DIR}/remote_config/response_generated.h
    src/desktop/rest.cc
    src/desktop/config_data.cc
    src/desktop/file_manager.cc
    src/desktop/metadata.cc
    src/desktop/notification_channel.cc
    src/desktop/remote_config_desktop.cc
    src/desktop/remote_config_request.cc
    src/desktop/remote_config_response.cc
)

if(ANDROID)
  set(remote_config_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(remote_config_platform_SRCS
      "${ios_SRCS}")
else()
  set(remote_config_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS OR use_stub)
  set(additional_include_DIR)
  set(additional_link_LIB)
else()
  set(additional_include_DIR
      ${FLATBUFFERS_SOURCE_DIR}/include
      ${PROJECT_BINARY_DIR}
      ${PROJECT_BINARY_DIR}/..)
  set(additional_link_LIB
      firebase_rest_lib)
endif()

add_library(firebase_remote_config STATIC
    ${common_SRCS}
    ${remote_config_platform_SRCS})

set_property(TARGET firebase_remote_config PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_remote_config
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB})
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_remote_config
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${additional_include_DIR}
)
target_compile_definitions(firebase_remote_config
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
  firebase_cpp_proguard_file(remote_config)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC) and Bitcode.
  target_compile_options(firebase_remote_config
                         PUBLIC "-fobjc-arc" "-fembed-bitcode")
  target_link_libraries(firebase_remote_config
                        PUBLIC "-fembed-bitcode")

  setup_pod_headers(
    firebase_remote_config
    POD_NAMES
      FirebaseCore
      FirebaseRemoteConfig
  )

  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_remote_config PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_remote_config "")
cpp_pack_public_headers()
