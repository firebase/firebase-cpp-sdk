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

# Build the generated header from the flatbuffer schema files.
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-generated"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
set(flatbuffer_schemas
    ${CMAKE_CURRENT_LIST_DIR}/src/android/schemas/messaging.fbs)
# Because of a bug in the version of Flatbuffers we are pinned to,
# additional flags need to be set.
set(FLATC_ARGS "${FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS}")
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

set_property(TARGET firebase_messaging PROPERTY FOLDER "Firebase Cpp")

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
      ${FLATBUFFERS_SOURCE_DIR}/include
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

if(ANDROID)
  firebase_cpp_proguard_file(messaging)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_messaging
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  setup_pod_headers(
    firebase_messaging
    POD_NAMES
      FirebaseCore
      FirebaseMessaging
  )
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_messaging "")
cpp_pack_public_headers()
