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

# CMake file for the firebase_functions library

# Common source files used by all platforms
set(common_SRCS
    src/common/callable_reference.cc
    src/common/common.cc
    src/common/functions.cc)

if(ANDROID)
  # Source files used by the Android implementation.
  set(functions_platform_SRCS
      src/android/callable_reference_android.cc
      src/android/functions_android.cc)
elseif(IOS)
  # Source files used by the iOS implementation.
  set(functions_platform_SRCS
      src/ios/callable_reference_ios.mm
      src/ios/functions_ios.mm)
else()
  # Source files used by the desktop implementation.
  set(functions_platform_SRCS
      src/desktop/callable_reference_desktop.cc
      src/desktop/functions_desktop.cc
      src/desktop/serialization.cc)
endif()

if(ANDROID OR IOS)
  set(additional_link_LIB)
  set(additional_target_DIR)
else()
  set(additional_link_LIB
      firebase_rest_lib)
  set(additional_target_DIR
      ${FLATBUFFERS_SOURCE_DIR}/include)
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

target_include_directories(firebase_functions
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${FIREBASE_SPM_SWIFT_HEADERS_DIR}
    ${additional_target_DIR}
)

target_compile_definitions(firebase_functions
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
)

if(ANDROID)
  firebase_cpp_proguard_file(functions)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC) for Objective-C/Objective-C++ sources.
  target_compile_options(firebase_functions
                         PRIVATE "$<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc>")

  setup_spm_headers(
    firebase_functions
    MODULES
      FirebaseCore
      FirebaseFunctions
  )

  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_functions PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_functions "")
cpp_pack_public_headers()
