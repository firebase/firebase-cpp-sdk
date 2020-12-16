# Copyright 2020 Google
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

# CMake file for the firebase_installations library

# Common source files used by all platforms
set(common_SRCS
    src/installations.cc)

# Source files used by the Android implementation.
set(android_SRCS
    src/android/installations_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/installations_ios.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    src/stub/installations_stub.cc)

if(ANDROID)
  set(installations_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(installations_platform_SRCS
      "${ios_SRCS}")
else()
  set(installations_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS OR use_stub)
  set(additional_link_LIB)
else()
  set(additional_link_LIB
      firebase_installations_desktop_impl)
endif()

add_library(firebase_installations STATIC
    ${common_SRCS}
    ${installations_platform_SRCS})

set_property(TARGET firebase_installations PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_installations
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_installations
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_installations
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
  firebase_cpp_proguard_file(installations)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_installations
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  setup_pod_headers(
    firebase_installations
    POD_NAMES
      FirebaseCore
      FirebaseInstallations
  )
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_installations "")
cpp_pack_public_headers()
