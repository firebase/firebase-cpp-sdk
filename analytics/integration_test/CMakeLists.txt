# Copyright 2020 Google LLC
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

# Cmake file for a single C++ integration test build.

cmake_minimum_required(VERSION 3.22)

find_program(FIREBASE_PYTHON_EXECUTABLE
  NAMES python3 python
  DOC "The Python interpreter to use, such as one from a venv"
  REQUIRED
)

# User settings for Firebase integration tests.
# Path to Firebase SDK.
# Try to read the path to the Firebase C++ SDK from an environment variable.
if (NOT "$ENV{FIREBASE_CPP_SDK_DIR}" STREQUAL "")
  set(DEFAULT_FIREBASE_CPP_SDK_DIR "$ENV{FIREBASE_CPP_SDK_DIR}")
else()
  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../cpp_sdk_version.json")
    set(DEFAULT_FIREBASE_CPP_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
  else()
    set(DEFAULT_FIREBASE_CPP_SDK_DIR "firebase_cpp_sdk")
  endif()
endif()
if ("${FIREBASE_CPP_SDK_DIR}" STREQUAL "")
  set(FIREBASE_CPP_SDK_DIR ${DEFAULT_FIREBASE_CPP_SDK_DIR})
endif()
if(NOT EXISTS ${FIREBASE_CPP_SDK_DIR})
  message(FATAL_ERROR "The Firebase C++ SDK directory does not exist: ${FIREBASE_CPP_SDK_DIR}. See the readme.md for more information")
endif()

# Copy all prerequisite files for integration tests to run.
if(NOT ANDROID)
  if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/../../setup_integration_tests.py)
    # If this is running from inside the SDK directory, run the setup script.
    execute_process(
            COMMAND
            ${FIREBASE_PYTHON_EXECUTABLE}
            "${CMAKE_CURRENT_LIST_DIR}/../../setup_integration_tests.py"
            "${CMAKE_CURRENT_LIST_DIR}"
            RESULT_VARIABLE
            FIREBASE_PYTHON_EXECUTABLE_RESULT
    )
    if(NOT FIREBASE_PYTHON_EXECUTABLE_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to run setup_integration_tests.py")
    endif()
  endif()
endif()

# Windows runtime mode, either MD or MT depending on whether you are using
# /MD or /MT. For more information see:
# https://msdn.microsoft.com/en-us/library/2kzt1wy3.aspx
set(MSVC_RUNTIME_MODE MD)

project(firebase_testapp)

# Integration test source files.
set(FIREBASE_APP_FRAMEWORK_SRCS
  src/app_framework.cc
  src/app_framework.h
)

set(FIREBASE_TEST_FRAMEWORK_SRCS
  src/firebase_test_framework.h
  src/firebase_test_framework.cc
)

set(FIREBASE_INTEGRATION_TEST_SRCS
  src/integration_test.cc
)

# The include directory for the testapp.
include_directories(src)

# Firebase C++ SDK requires C++17.
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED YES)  # Don't fall back to an earlier version.

# Download and unpack googletest (and googlemock) at configure time
set(GOOGLETEST_ROOT ${CMAKE_CURRENT_LIST_DIR}/external/googletest)
# Note: Once googletest is downloaded once, it won't be updated or
# downloaded again unless you delete the "external/googletest"
# directory.
if (NOT EXISTS ${GOOGLETEST_ROOT}/src/googletest/src/gtest-all.cc)
  configure_file(googletest.cmake
    ${CMAKE_CURRENT_LIST_DIR}/external/googletest/CMakeLists.txt COPYONLY)
  execute_process(COMMAND ${CMAKE_COMMAND} .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/external/googletest )
  if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/external/googletest )
  if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
  endif()
endif()

if(ANDROID)
  # Build an Android application.

  # Source files used for the Android build.
  set(FIREBASE_APP_FRAMEWORK_ANDROID_SRCS
    src/android/android_app_framework.cc
  )

  # Source files used for the Android build.
  set(FIREBASE_TEST_FRAMEWORK_ANDROID_SRCS
    src/android/android_firebase_test_framework.cc
  )

  # Build native_app_glue as a static lib
  add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

  # Export ANativeActivity_onCreate(),
  # Refer to: https://github.com/android-ndk/ndk/issues/381.
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

  add_library(gtest STATIC
    ${GOOGLETEST_ROOT}/src/googletest/src/gtest-all.cc)
  target_include_directories(gtest
    PRIVATE ${GOOGLETEST_ROOT}/src/googletest
    PUBLIC ${GOOGLETEST_ROOT}/src/googletest/include)
  add_library(gmock STATIC
    ${GOOGLETEST_ROOT}/src/googlemock/src/gmock-all.cc)
  target_include_directories(gmock
    PRIVATE ${GOOGLETEST_ROOT}/src/googletest
    PRIVATE ${GOOGLETEST_ROOT}/src/googlemock
    PUBLIC ${GOOGLETEST_ROOT}/src/googletest/include
    PUBLIC ${GOOGLETEST_ROOT}/src/googlemock/include)

  # Define the target as a shared library, as that is what gradle expects.
  set(integration_test_target_name "android_integration_test_main")
  add_library(${integration_test_target_name} SHARED
    ${FIREBASE_APP_FRAMEWORK_SRCS}
    ${FIREBASE_APP_FRAMEWORK_ANDROID_SRCS}
    ${FIREBASE_INTEGRATION_TEST_SRCS}
    ${FIREBASE_TEST_FRAMEWORK_SRCS}
    ${FIREBASE_TEST_FRAMEWORK_ANDROID_SRCS}
  )

  target_include_directories(${integration_test_target_name} PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue)

  set(ADDITIONAL_LIBS log android atomic native_app_glue)
else()
  # Build a desktop application.
  add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

  # Prevent overriding the parent project's compiler/linker
  # settings on Windows
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  # Add googletest directly to our build. This defines
  # the gtest and gtest_main targets.
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/googletest/src
                   ${CMAKE_CURRENT_LIST_DIR}/external/googletest/build
                   EXCLUDE_FROM_ALL)

  # The gtest/gtest_main targets carry header search path
  # dependencies automatically when using CMake 2.8.11 or
  # later. Otherwise we have to add them here ourselves.
  if (CMAKE_VERSION VERSION_LESS 2.8.11)
    include_directories("${gtest_SOURCE_DIR}/include")
    include_directories("${gmock_SOURCE_DIR}/include")
  endif()

  # Windows runtime mode, either MD or MT depending on whether you are using
  # /MD or /MT. For more information see:
  # https://msdn.microsoft.com/en-us/library/2kzt1wy3.aspx
  set(MSVC_RUNTIME_MODE MD)

  # Platform abstraction layer for the desktop integration test.
  set(FIREBASE_APP_FRAMEWORK_DESKTOP_SRCS
    src/desktop/desktop_app_framework.cc
    src/desktop/desktop_firebase_test_framework.cc
  )

  set(integration_test_target_name "integration_test")
  add_executable(${integration_test_target_name}
    ${FIREBASE_APP_FRAMEWORK_SRCS}
    ${FIREBASE_APP_FRAMEWORK_DESKTOP_SRCS}
    ${FIREBASE_TEST_FRAMEWORK_SRCS}
    ${FIREBASE_INTEGRATION_TEST_SRCS}
  )

  if(APPLE)
    set(ADDITIONAL_LIBS
      gssapi_krb5
      pthread
      "-framework CoreFoundation"
      "-framework Foundation"
      "-framework GSS"
      "-framework Security"
    )
  elseif(MSVC)
    set(ADDITIONAL_LIBS advapi32 ws2_32 crypt32)
    set(ANALYTICS_WINDOWS_DLL "${FIREBASE_CPP_SDK_DIR}/analytics/windows/google_analytics.dll")

    # For Windows, check if the Analytics DLL exists, and copy it in if so.
    if (EXISTS "${ANALYTICS_WINDOWS_DLL}")
      add_custom_command(
        TARGET ${integration_test_target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
	  "${ANALYTICS_WINDOWS_DLL}"
          $<TARGET_FILE_DIR:${integration_test_target_name}>)
    endif()
  else()
    set(ADDITIONAL_LIBS pthread)
  endif()

  # If a GoogleService-Info.plist is present without google-services-desktop.json,
  # convert it automatically so desktop App::Create() can load the Apple config.
  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/GoogleService-Info.plist" AND
     NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/google-services-desktop.json")
    execute_process(
      COMMAND ${FIREBASE_PYTHON_EXECUTABLE}
        "${FIREBASE_CPP_SDK_DIR}/generate_xml_from_google_services_json.py"
        --plist
        -i "${CMAKE_CURRENT_LIST_DIR}/GoogleService-Info.plist"
        -o "${CMAKE_CURRENT_LIST_DIR}/google-services-desktop.json"
      RESULT_VARIABLE plist_convert_result
    )
  endif()

  # If a config file is present, copy it into the binary location so that it's
  # possible to create the default Firebase app.
  set(FOUND_JSON_FILE FALSE)
  set(SELECTED_CONFIG_FILE "")
  foreach(config "google-services-desktop.json" "google-services.json")
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/${config}")
      set(SELECTED_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/${config}")
      add_custom_command(
        TARGET ${integration_test_target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
          "${SELECTED_CONFIG_FILE}" $<TARGET_FILE_DIR:${integration_test_target_name}>/google-services-desktop.json)
      set(FOUND_JSON_FILE TRUE)
      break()
    endif()
  endforeach()
  if(NOT FOUND_JSON_FILE)
    message(WARNING "Failed to find either google-services-desktop.json or google-services.json. See the readme.md for more information.")
  endif()
endif()

# Add the Firebase libraries to the target using the function from the SDK.
add_subdirectory(${FIREBASE_CPP_SDK_DIR} bin/ EXCLUDE_FROM_ALL)
# Note that firebase_app needs to be last in the list.
set(firebase_libs firebase_analytics firebase_installations firebase_app)
set(gtest_libs gtest gmock)
target_link_libraries(${integration_test_target_name} ${firebase_libs}
  ${gtest_libs} ${ADDITIONAL_LIBS})

# On macOS desktop, optionally embed the Firebase Apple SDK so Analytics and
# Installations use it instead of the desktop stubs. Requires a
# google-services-desktop.json generated from an Apple app's
# GoogleService-Info.plist (App ID "1:...:ios:...").
option(FIREBASE_ANALYTICS_TEST_EMBED_APPLE_SDK
       "Embed the Firebase Apple SDK in the macOS desktop integration test." OFF)
if(APPLE AND NOT IOS AND FIREBASE_ANALYTICS_TEST_EMBED_APPLE_SDK)
  set(DEFAULT_ANALYTICS_BUNDLE_ID "com.google.firebase.cpp.analytics.testapp")
  if(SELECTED_CONFIG_FILE AND EXISTS "${SELECTED_CONFIG_FILE}")
    execute_process(
      COMMAND ${FIREBASE_PYTHON_EXECUTABLE}
        "${FIREBASE_CPP_SDK_DIR}/generate_xml_from_google_services_json.py"
        -l -i "${SELECTED_CONFIG_FILE}"
      OUTPUT_VARIABLE DETECTED_BUNDLE_ID
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(DETECTED_BUNDLE_ID)
      string(REGEX REPLACE "\n.*$" "" DETECTED_BUNDLE_ID "${DETECTED_BUNDLE_ID}")
      set(DEFAULT_ANALYTICS_BUNDLE_ID "${DETECTED_BUNDLE_ID}")
    endif()
  endif()
  set(FIREBASE_ANALYTICS_TEST_BUNDLE_ID "${DEFAULT_ANALYTICS_BUNDLE_ID}"
      CACHE STRING "Bundle ID of the macOS desktop integration test app.")
  # GoogleAppMeasurement expects to run inside an app bundle with a bundle ID.
  set_target_properties(${integration_test_target_name} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "${FIREBASE_ANALYTICS_TEST_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "Firebase Analytics Test"
  )
  firebase_cpp_link_apple_analytics_sdk(${integration_test_target_name})
  target_compile_definitions(${integration_test_target_name}
    PRIVATE FIREBASE_ANALYTICS_TEST_APPLE_SDK_EMBEDDED=1)

  # Standalone live verification app (`macos_live_demo.app`) that enables
  # DebugView, prints the real Installation ID and Analytics IDs, logs events,
  # and runs the macOS CFRunLoop so events appear in Firebase Console DebugView.
  add_executable(macos_live_demo MACOSX_BUNDLE
    src/macos_live_demo.cc
  )
  set_target_properties(macos_live_demo PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "${FIREBASE_ANALYTICS_TEST_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "Firebase macOS Live Demo"
  )
  target_link_libraries(macos_live_demo ${firebase_libs} ${ADDITIONAL_LIBS})
  firebase_cpp_link_apple_analytics_sdk(macos_live_demo)
  if(SELECTED_CONFIG_FILE AND EXISTS "${SELECTED_CONFIG_FILE}")
    add_custom_command(
      TARGET macos_live_demo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
        "${SELECTED_CONFIG_FILE}" $<TARGET_FILE_DIR:macos_live_demo>/google-services-desktop.json
    )
  endif()
endif()
