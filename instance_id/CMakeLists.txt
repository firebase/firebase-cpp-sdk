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

# CMake file for the firebase_instance_id library

# Common source files used by all platforms
set(common_SRCS
    src/instance_id.cc
    src/instance_id_internal_base.cc)

# Source files used by the Android implementation.
set(android_SRCS
    src/android/instance_id.cc
    src/android/instance_id_internal.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/instance_id.mm
    src/ios/instance_id_internal.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    src/desktop/instance_id.cc
    src/desktop/instance_id_internal.cc)

if(ANDROID)
  set(instance_id_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(instance_id_platform_SRCS
      "${ios_SRCS}")
else()
  set(instance_id_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS OR use_stub)
  set(additional_link_LIB)
else()
  set(additional_link_LIB
      firebase_instance_id_desktop_impl)
endif()

add_library(firebase_instance_id STATIC
    ${common_SRCS}
    ${instance_id_platform_SRCS})

set_property(TARGET firebase_instance_id PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_instance_id
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_instance_id
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_instance_id
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
  firebase_cpp_proguard_file(instance_id)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_instance_id
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  setup_pod_headers(
    firebase_instance_id
    POD_NAMES
      FirebaseCore
      FirebaseInstanceID
  )
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_instance_id "")
cpp_pack_public_headers()
