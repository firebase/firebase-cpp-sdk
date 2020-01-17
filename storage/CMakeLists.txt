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

# CMake file for the firebase_storage library

# Common source files used by all platforms
set(common_SRCS
    src/common/common.cc
    src/common/controller.cc
    src/common/listener.cc
    src/common/metadata.cc
    src/common/storage.cc
    src/common/storage_reference.cc
    src/common/storage_uri_parser.cc)

# Define the resource build needed for Android
firebase_cpp_gradle(":storage:storage_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/storage_resources/build/dexed.jar")
binary_to_array("storage_resources"
    "${CMAKE_CURRENT_LIST_DIR}/storage_resources/build/dexed.jar"
    "firebase_storage_resources"
    "${FIREBASE_GEN_FILE_DIR}/storage")

# Source files used by the Android implementation.
set(android_SRCS
    ${storage_resources_source}
    src/android/controller_android.cc
    src/android/metadata_android.cc
    src/android/storage_android.cc
    src/android/storage_reference_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/controller_ios.mm
    src/ios/listener_ios.mm
    src/ios/metadata_ios.mm
    src/ios/storage_ios.mm
    src/ios/storage_reference_ios.mm
    src/ios/util_ios.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    src/desktop/controller_desktop.cc
    src/desktop/curl_requests.cc
    src/desktop/listener_desktop.cc
    src/desktop/metadata_desktop.cc
    src/desktop/rest_operation.cc
    src/desktop/storage_desktop.cc
    src/desktop/storage_path.cc
    src/desktop/storage_reference_desktop.cc)

if(ANDROID)
  set(storage_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(storage_platform_SRCS
      "${ios_SRCS}")
else()
  set(storage_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS)
  set(additional_link_LIB)
  set(additional_DEFINES)
else()
  set(additional_link_LIB
      firebase_rest_lib)

  set(additional_DEFINES
      -DFIREBASE_TARGET_DESKTOP=1)
endif()

add_library(firebase_storage STATIC
    ${common_SRCS}
    ${storage_platform_SRCS})

set_property(TARGET firebase_storage PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_storage
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_storage
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_storage
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
  firebase_cpp_proguard_file(storage)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC)
  set_property(
    TARGET firebase_storage
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  # Empty entry to point to the GTMSessionFetcher folder '.'
  setup_pod_headers(
    firebase_storage
    POD_NAMES
      .
      FirebaseCore
      FirebaseStorage
  )
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_storage "")
cpp_pack_public_headers()
