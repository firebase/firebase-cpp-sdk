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

# CMake file for the firebase_functions library

# Common source files used by all platforms
set(common_SRCS
    src/common/callable_reference.cc
    src/common/callable_result.cc
    src/common/common.cc
    src/common/functions.cc)

# Source files used by the Android implementation.
set(android_SRCS
    src/android/callable_reference_android.cc
    src/android/functions_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/callable_reference_ios.mm
    src/ios/functions_ios.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    src/desktop/callable_reference_desktop.cc
    src/desktop/functions_desktop.cc
    src/desktop/serialization.cc)

if(ANDROID)
  set(functions_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(functions_platform_SRCS
      "${ios_SRCS}")
else()
  set(functions_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS)
  set(additional_link_LIB)
else()
  set(additional_link_LIB
      firebase_rest_lib)
endif()

add_library(firebase_functions STATIC
    ${common_SRCS}
    ${functions_platform_SRCS})

set_property(TARGET firebase_functions PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_functions
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_functions
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_functions
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
  firebase_cpp_proguard_file(functions)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_functions
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  setup_pod_headers(
    firebase_functions
    POD_NAMES
      FirebaseCore
      FirebaseFunctions
  )
endif()

cpp_pack_library(firebase_functions "")
cpp_pack_public_headers()
