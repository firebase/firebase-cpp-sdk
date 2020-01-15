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

include(CMakeParseArguments)

# cc_test(
#   target
#   SOURCES sources...
#   DEPENDS libraries...
# )
#
# Defines a new test executable target with the given target name, sources, and
# dependencies.  Implicitly adds DEPENDS on gtest and gtest_main.
function(cc_test name)
  if (ANDROID OR IOS)
    return()
  endif()

  set(multi DEPENDS SOURCES INCLUDES DEFINES)
  # Parse the arguments into cc_test_SOURCES and cc_test_DEPENDS.
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

# cc_test_on_ios(
#   target
#   SOURCES sources...
#   DEPENDS libraries...
#   CUSTOM_FRAMEWORKS frameworks...
# )
#
# Defines a new test executable target with the given target name, sources, and
# dependencies.  Implicitly adds DEPENDS on gtest, gtest_main, Foundation and
# CoreFoundation frameworks. CUSTOM_FRAMEWORKS will include any custom Cocoapods
# frameworks beyond the baseline FirebaseAnalytics.
function(cc_test_on_ios name)
  if (NOT IOS)
    return()
  endif()

  set(multi DEPENDS SOURCES INCLUDES DEFINES CUSTOM_FRAMEWORKS)
  # Parse the arguments into cc_test_SOURCES and cc_test_DEPENDS.
  cmake_parse_arguments(cc_test "" "" "${multi}" ${ARGN})

  set(SDK_FRAMEWORK_DIR_NAMES 
    FirebaseABTesting
    FirebaseAnalytics
    FirebaseAuth
    FirebaseDatabase
    FirebaseDynamicLinks
    FirebaseFunctions
    FirebaseMessaging
    FirebaseRemoteConfig
    FirebaseStorage
    GoogleSignIn
  )

  # The build environment can use a user-downloaded Firebase IOS SDK defined by
  # FIREBASE_IOS_SDK_DIR.  Alternatively download the SDK directly if it's not
  # configured and store the files in external/firebase_ios_sdk.
  if(NOT DEFINED ${FIREBASE_IOS_SDK_DIR})
    set(FIREBASE_IOS_SDK_DIR 
        "${CMAKE_BINARY_DIR}/external/firebase_ios_sdk/Firebase")
  endif()

  # Add the directories of the SDK download to the framework searchpath.
  foreach(FRAMEWORK IN LISTS SDK_FRAMEWORK_DIR_NAMES)
    set(directory "${FIREBASE_IOS_SDK_DIR}/${FRAMEWORK}")
    LIST(APPEND FRAMEWORK_DIRS "-F ${directory}")
  endforeach()

  # All Firebase SDK modules require the Firebase Analytics Frameworks
  # so include them by default:
  set(DEFAULT_FRAMEWORKS
    FIRAnalyticsConnector
    FirebaseAnalytics
    FirebaseCore
    FirebaseCoreDiagnostics
    FirebaseInstanceID
    GTMSessionFetcher
    GoogleAppMeasurement
    GoogleDataTransport
    GoogleDataTransportCCTSupport
    GoogleUtilities
    nanopb
  )

  # Construct a command line list of frameworks from the default frameworks
  # (above) and for those passed to this function through the CUSTOM_FRAMEWORKS
  # parameter.
  foreach(FRAMEWORK IN LISTS DEFAULT_FRAMEWORKS cc_test_CUSTOM_FRAMEWORKS)
    LIST(APPEND FRAMEWORK_INCLUDES "-framework ${FRAMEWORK}")
  endforeach()

  list(APPEND cc_test_DEPENDS
       gmock
       gtest
       gtest_main
       "${FRAMEWORK_DIRS}"
       "-framework CoreFoundation"
       "-framework Foundation"
       "${FRAMEWORK_INCLUDES}"
  )

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
      -DDEATHTEST_SIGABRT="SIGABRT"
  )
endfunction()
