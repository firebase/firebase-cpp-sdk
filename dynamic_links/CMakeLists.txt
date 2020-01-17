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

# CMake file for the firebase_dynamic_links library

# Common source files used by all platforms
set(common_SRCS
    src/common.cc
    src/common.h
    src/listener.cc)

# Source files used by the Android implementation.
set(android_SRCS
    src/dynamic_links_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/dynamic_links_ios.mm)

# Source files used by the stub implementation.
set(stub_SRCS
    src/dynamic_links_stub.cc)

if(ANDROID)
  set(dynamic_links_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(dynamic_links_platform_SRCS
      "${ios_SRCS}")
else()
  set(dynamic_links_platform_SRCS
      "${stub_SRCS}")
endif()

add_library(firebase_dynamic_links STATIC
    ${common_SRCS}
    ${dynamic_links_platform_SRCS})

set_property(TARGET firebase_dynamic_links PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_dynamic_links
    firebase_app)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_dynamic_links
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_dynamic_links
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
  firebase_cpp_proguard_file(dynamic_links)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_dynamic_links
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  setup_pod_headers(
    firebase_dynamic_links
    POD_NAMES
      FirebaseDynamicLinks
  )
endif()

cpp_pack_library(firebase_dynamic_links "")
cpp_pack_public_headers()
