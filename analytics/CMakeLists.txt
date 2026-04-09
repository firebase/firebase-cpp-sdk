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

# CMake file for the firebase_analytics library

# Analytics generates header files for default events, parameters, and
# properties based on the iOS SDK, that are used across all platforms.

set(analytics_generated_headers_dir
    "${FIREBASE_GEN_FILE_DIR}/analytics/src/include/firebase/analytics")
set(event_names_header "${analytics_generated_headers_dir}/event_names.h")
set(parameter_names_header
    "${analytics_generated_headers_dir}/parameter_names.h")
set(user_property_names_header
    "${analytics_generated_headers_dir}/user_property_names.h")
file(MAKE_DIRECTORY ${analytics_generated_headers_dir})

# Generate the header file by invoking the generate_constants python script.
function(generate_analytics_header OBJC_FILE CPP_FILE)
  add_custom_command(
    OUTPUT ${CPP_FILE}
    COMMAND ${FIREBASE_PYTHON_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/generate_constants.py"
      "--objc_header=${OBJC_FILE}"
      "--cpp_header=${CPP_FILE}"
    DEPENDS ${OBJC_FILE}
    COMMENT "Generating ${CPP_FILE}"
  )
endfunction()

# Call the above function for all of the header files to generate.
generate_analytics_header(
  "${CMAKE_CURRENT_LIST_DIR}/ios_headers/FIREventNames.h"
  "${event_names_header}"
)
generate_analytics_header(
  "${CMAKE_CURRENT_LIST_DIR}/ios_headers/FIRParameterNames.h"
  "${parameter_names_header}"
)
generate_analytics_header(
  "${CMAKE_CURRENT_LIST_DIR}/ios_headers/FIRUserPropertyNames.h"
  "${user_property_names_header}"
)
add_custom_target(FIREBASE_ANALYTICS_GENERATED_HEADERS
  DEPENDS 
  "${user_property_names_header}"
  "${parameter_names_header}"
  "${event_names_header}"
)

# Common source files used by all platforms
set(common_SRCS
    src/analytics_common.cc
    # List the generated files, so that they are generated for the build.
    ${event_names_header}
    ${parameter_names_header}
    ${user_property_names_header})

# Source files used by the Android implementation.
set(android_SRCS
    src/analytics_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/analytics_ios.mm
    src/ios/swift/StoreKit2Bridge.swift)

# Source files used by the desktop / stub implementation.
set(desktop_SRCS
    src/analytics_desktop.cc
    src/analytics_desktop_dynamic.c)

if(WIN32)
  # Add Windows-specific sources for desktop builds.
  list(APPEND desktop_SRCS src/analytics_windows.cc)
endif()

if(ANDROID)
  set(analytics_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    message(FATAL_ERROR "Swift is not supported by the 'Unix Makefiles' generator on iOS. Please use the Xcode generator (-G Xcode) or Ninja (-G Ninja).")
  endif()
  set(analytics_platform_SRCS
      "${ios_SRCS}")
  
else()
  set(analytics_platform_SRCS
      "${desktop_SRCS}")
endif()

add_library(firebase_analytics STATIC
    ${common_SRCS}
    ${analytics_platform_SRCS})


add_dependencies(firebase_analytics FIREBASE_ANALYTICS_GENERATED_HEADERS)

set_property(TARGET firebase_analytics PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_analytics
    PUBLIC firebase_app)
if(WIN32)
  target_link_libraries(firebase_analytics PRIVATE Crypt32.lib)
endif()
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_analytics
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
    ${FIREBASE_GEN_FILE_DIR}/analytics/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${FIREBASE_GEN_FILE_DIR}
)
target_compile_definitions(firebase_analytics
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
)
# Automatically include headers that might not be declared.
if(MSVC)
  target_compile_options(firebase_analytics PRIVATE
    $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:/FI"assert.h">
    $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:/FI"string.h">
    $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:/FI"stdint.h">)
else()
  target_compile_options(firebase_analytics PRIVATE
    $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:SHELL:-include assert.h -include string.h>
  )
endif()

if(ANDROID)
  firebase_cpp_proguard_file(analytics)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC) and Bitcode specifically for Objective-C++ files.
  # Note: -fembed-bitcode is placed here for src/analytics_ios.mm so that it is not passed
  # to the Swift compiler, which does not support the flag.
  set_source_files_properties(src/analytics_ios.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc;-fembed-bitcode")

  target_include_directories(firebase_analytics
    PRIVATE
      "$(DERIVED_FILE_DIR)"
  )
 
if(IOS OR TVOS)
  # Swift needs to find the FirebaseAnalytics module from CocoaPods
  set(pods_dir "${FIREBASE_POD_DIR}/Pods")
  
  # Point to the base directories containing the .xcframework folders.
  # Xcode natively handles XCFrameworks and will pick the right slice automatically.
  # Determine the xcframework architecture slice based on the target platform
  # and if it is running on simulator or device.
  if(IOS)
    string(TOLOWER "${CMAKE_OSX_SYSROOT}" sysroot_lower)
    if(sysroot_lower MATCHES "simulator")
      set(analytics_slice "ios-arm64_x86_64-simulator")
    else()
      set(analytics_slice "ios-arm64")
    endif()
  elseif(TVOS)
    string(TOLOWER "${CMAKE_OSX_SYSROOT}" sysroot_lower)
    if(sysroot_lower MATCHES "simulator")
      set(analytics_slice "tvos-arm64_x86_64-simulator")
    else()
      set(analytics_slice "tvos-arm64")
    endif()
  endif()

  set(analytics_framework_dir "${pods_dir}/FirebaseAnalytics/Frameworks/FirebaseAnalytics.xcframework/${analytics_slice}")
  set(measurement_framework_dir "${pods_dir}/GoogleAppMeasurement/Frameworks/GoogleAppMeasurement.xcframework/${analytics_slice}")
  
  target_compile_options(firebase_analytics
    PRIVATE
      $<$<COMPILE_LANGUAGE:Swift>:-F${analytics_framework_dir}>
      $<$<COMPILE_LANGUAGE:Swift>:-F${measurement_framework_dir}>
  )
  
  target_link_options(firebase_analytics
    PUBLIC
      "-F${analytics_framework_dir}"
      "-F${measurement_framework_dir}"
  )
  
  # Prevent Xcode from trying to build or evaluate headers for unused architectures
  set_target_properties(firebase_analytics PROPERTIES
    XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "YES"
  )
endif()

  setup_pod_headers(
    firebase_analytics
    POD_NAMES
      FirebaseCore
      FirebaseAnalytics
  )

  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_analytics PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_analytics "")
cpp_pack_public_headers()
