# Copyright 2021 Google LLC
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

# CMake file for the firebase_ump library

# Common source files used by all platforms
set(common_SRCS
    src/common/consent_info.cc
    src/common/consent_info_internal.cc
)

# Define the resource build needed for Android
firebase_cpp_gradle(":ump:ump_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/ump_resources/build/ump_resources_lib.jar")
binary_to_array("ump_resources"
    "${CMAKE_CURRENT_LIST_DIR}/ump_resources/build/ump_resources_lib.jar"
    "firebase_ump"
    "${FIREBASE_GEN_FILE_DIR}/ump")

# Source files used by the Android implementation.
set(android_SRCS
    ${ump_resources_source}
    src/android/consent_info_internal_android.cc
)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/consent_info_internal_ios.mm
)

# Source files used by the stub implementation.
set(stub_SRCS
    src/stub/consent_info_internal_stub.cc
)

if(ANDROID)
  set(ump_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(ump_platform_SRCS
      "${ios_SRCS}")
else()
  set(ump_platform_SRCS
      "${stub_SRCS}")
endif()

add_library(firebase_ump STATIC
    ${common_SRCS}
    ${ump_platform_SRCS})

set_property(TARGET firebase_ump PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_ump
    PUBLIC firebase_app)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_ump
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)
target_compile_definitions(firebase_ump
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
  firebase_cpp_proguard_file(ump)
elseif(IOS)
  # UMP for iOS uses weak references, which requires enabling Automatic
  # Reference Counting (ARC). Also enable BitCode.
  target_compile_options(firebase_ump
                         PUBLIC "-fobjc-arc" "-fembed-bitcode")
  target_link_libraries(firebase_ump
                        PUBLIC "-fembed-bitcode")

  setup_pod_headers(
    firebase_ump
    POD_NAMES
      GoogleUserMessagingPlatform
  )

  # UMP expects the header files to be in a subfolder, so set up a symlink to
  # accomplish that.
  symlink_pod_headers(firebase_ump GoogleUserMessagingPlatform UserMessagingPlatform)

  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_ump PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

cpp_pack_library(firebase_ump "")
cpp_pack_public_headers()
