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

include(CMakeParseArguments)

# firebase_cpp_cc_test(
#   target
#   SOURCES sources...
#   DEPENDS libraries...
#   INCLUDES include directories...
#   DEFINES definitions...
# )
#
# Defines a new test executable target with the given target name, sources, and
# dependencies.  Implicitly adds DEPENDS on gtest and gtest_main.
function(firebase_cpp_cc_test name)
  if (ANDROID OR IOS)
    return()
  endif()

  set(multi DEPENDS SOURCES INCLUDES DEFINES)
  # Parse the arguments into cc_test_SOURCES, ..._DEPENDS, etc.
  cmake_parse_arguments(cc_test "" "" "${multi}" ${ARGN})

  list(APPEND cc_test_DEPENDS gmock gtest gtest_main)

  # Include Foundation and Security frameworks, as multiple tests require them
  # when running on Mac.
  if (APPLE)
    list(APPEND cc_test_DEPENDS
         "-framework Foundation"
         "-framework Security")
  endif()

  add_executable(${name} ${cc_test_SOURCES})
  add_test(${name} ${name})
  target_include_directories(${name}
    PRIVATE
      ${FIREBASE_SOURCE_DIR}
      ${cc_test_INCLUDES}
  )
  target_link_libraries(${name} PRIVATE ${cc_test_DEPENDS})
  target_compile_definitions(${name}
    PRIVATE
      -DINTERNAL_EXPERIMENTAL=1
      ${cc_test_DEFINES}
      -DDEATHTEST_SIGABRT=""
  )
endfunction()

# ios_test_add_frameworks(
#   target
#   CUSTOM_FRAMEWORKS ...
# )
#
# Rolls up a list of the required iOS frameworks for building Firebase
# apps for iOS.  Implicitly adds DEPENDS on FirebaseAnalytics framework
# requirements, which is the base requirements for all Firebase SDKs.
# CUSTOM_FRAMEWORKS will be added to this list.
# The result of this function sets FRAMEWORK_DIRS and FRAMEWORK_INCLUDES
# in the parent scope. These variables should be passed to target_link_libraries
# for linking the final app.
function(ios_test_add_frameworks name)
  set(multi CUSTOM_FRAMEWORKS)
  cmake_parse_arguments(ios_test_add_frameworks "" "" "${multi}" ${ARGN})

  if(NOT DEFINED ${FIREBASE_IOS_SDK_DIR})
    set(FIREBASE_IOS_SDK_DIR
        "${CMAKE_BINARY_DIR}/external/firebase_ios_sdk/Firebase")
  endif()

  set(SDK_FRAMEWORK_DIR_NAMES FirebaseABTesting FirebaseAnalytics FirebaseAuth
                              FirebaseDatabase FirebaseDynamicLinks
                              FirebaseFunctions FirebaseMessaging
                              FirebaseRemoteConfig FirebaseStorage
                              GoogleSignIn )

  foreach(FRAMEWORK IN LISTS SDK_FRAMEWORK_DIR_NAMES)
    LIST(APPEND DIRS "-F ${FIREBASE_IOS_SDK_DIR}/${FRAMEWORK}")
  endforeach()

  # All Firebase SDK modules require the Firebase Analytics Frameworks
  # so include them by default:
  set(DEFAULT_FRAMEWORKS FIRAnalyticsConnector FirebaseAnalytics FirebaseCore
                         FirebaseCoreDiagnostics FirebaseInstanceID
                         GTMSessionFetcher GoogleAppMeasurement
                         GoogleDataTransport GoogleDataTransportCCTSupport
                         GoogleUtilities nanopb )

  foreach(FRAMEWORK IN LISTS DEFAULT_FRAMEWORKS
          ios_test_add_frameworks_CUSTOM_FRAMEWORKS)
    LIST(APPEND INCLUDES "-framework ${FRAMEWORK}")
  endforeach()

  set(FRAMEWORK_DIRS ${DIRS} PARENT_SCOPE)
  set(FRAMEWORK_INCLUDES ${INCLUDES} PARENT_SCOPE)
endfunction()

# firebase_cpp_cc_test_on_ios(
#   target
#   HOST host
#   SOURCES sources...
#   DEPENDS libraries...
#   CUSTOM_FRAMEWORKS frameworks...
#   DEFINES defines...
# )
#
# Defines a new test executable target with the given target name, sources, and
# dependencies.  Implicitly adds DEPENDS on gtest, gtest_main, Foundation and
# CoreFoundation frameworks and attaches them to the predefined HOST executable.
# CUSTOM_FRAMEWORKS will include any custom Cocoapods frameworks beyond the

# baseline FirebaseAnalytics.  DEFINES will be added to the
# target_compile_definitions call.
function(firebase_cpp_cc_test_on_ios name)
  if (NOT IOS)
    return()
  endif()

  enable_testing()
  find_package(XCTest REQUIRED)

  set(multi HOST DEPENDS SOURCES INCLUDES DEFINES CUSTOM_FRAMEWORKS)
  cmake_parse_arguments(cc_test "" "" "${multi}" ${ARGN})

  ios_test_add_frameworks(${name}
    CUSTOM_FRAMEWORKS
      ${cc_test_CUSTOM_FRAMEWORKS}
  )

  list(APPEND cc_test_DEPENDS
       gmock gtest gtest_main
  )

  xctest_add_bundle(
      ${name}
      ${cc_test_HOST}
      ${cc_test_SOURCES}
  )

  # Library Links
  target_link_libraries(
    ${name}
    PRIVATE
      ${cc_test_DEPENDS} ${FRAMEWORK_DIRS} ${FRAMEWORK_INCLUDES} objc
  )

  # Compile Options
  target_compile_options(
    ${name} PRIVATE
    ${FIREBASE_CXX_FLAGS}
  )

  # Include Paths
  target_include_directories(${name}
    PRIVATE
      ${FIREBASE_SOURCE_DIR}
      ${FIREBASE_SOURCE_DIR}/app/src/include
  )

  # Preprocessor Definitions
  target_compile_definitions(${name} PRIVATE
       -DINTERNAL_EXPERIMENTAL=1
       -DDEATHTEST_SIGABRT="SIGABRT"
       ${cc_test_DEFINES}
  )

  xctest_add_test(${name} ${name})
endfunction()
