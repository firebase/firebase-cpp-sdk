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

# Common source files used by all platforms
set(common_SRCS
    src/common.cc
    src/remote_config.cc)

# Source files used by the Android implementation.
set(android_SRCS
    src/android/remote_config_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/remote_config_ios.mm)

if (NOT PROTOBUF_FOUND)
  # Only log a message if building for Desktop, since mobile doesn't care.
  if (NOT ANDROID AND NOT IOS)
    message(FATAL_ERROR "Unable to find Protobuf, which is needed for Remote \
            Configs's desktop implementation. Install Protobuf and add it to \
            your PATH, or set the CMake option FIREBASE_INCLUDE_REMOTE_CONFIG \
            to OFF to disable the firebase_remote_config build target.")
  endif()
else()
  # Generates config.pb.c and config.pb.h
  NANOPB_GENERATE_CPP(PROTO_SRCS PROTO_HDRS src_protos/config.proto)
  set(desktop_SRCS
      ${PROTO_SRCS}
      ${PROTO_HDRS}
      src/desktop/remote_config_legacy.cc
      src/desktop/rest.cc
      src/desktop/config_data.cc
      src/desktop/file_manager.cc
      src/desktop/metadata.cc
      src/desktop/notification_channel.cc
      src/desktop/remote_config_desktop.cc
      src/desktop/rest_nanopb_decode.cc
      src/desktop/rest_nanopb_encode.cc
  )
endif()

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
      ${NANOPB_INCLUDE_DIRS}
      ${PROJECT_BINARY_DIR}
      ${PROJECT_BINARY_DIR}/..)
  set(additional_link_LIB
      firebase_rest_lib
      firebase_instance_id_desktop_impl
      flatbuffers)
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
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_remote_config
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  setup_pod_headers(
    firebase_remote_config
    POD_NAMES
      FirebaseCore
      FirebaseRemoteConfig
  )
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_remote_config "")
cpp_pack_public_headers()
