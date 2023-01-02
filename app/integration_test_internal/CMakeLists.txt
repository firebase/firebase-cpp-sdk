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

cmake_minimum_required(VERSION 2.8)

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

# Ensure min/max macros don't get declared on Windows
# (so we can use std::min/max), before including the Firebase subdirectories.
if(MSVC)
  add_definitions(-DNOMINMAX)
endif()

# Integration test source files.
set(FIREBASE_APP_FRAMEWORK_SRCS
  src/app_framework.cc
  src/app_framework.h
)

set(FIREBASE_TEST_FRAMEWORK_SRCS
  src/firebase_test_framework.h
  src/firebase_test_framework.cc
)

set(FIREBASE_INTEGRATION_TEST_PORTABLE_SRCS
  # Copy of the standard integration test source file.
  src/integration_test.cc
  # All of the unit tests from App and other SDKs.
  ${FIREBASE_CPP_SDK_DIR}/app/tests/assert_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/base64_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/callback_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/cleanup_notifier_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/flexbuffer_matcher_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/flexbuffer_matcher.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/future_manager_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/future_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/intrusive_list_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/locale_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/log_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/logger_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/optional_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/path_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/reference_count_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/scheduler_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/semaphore_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/thread_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/time_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/util_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/uuid_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/variant_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/tests/variant_util_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/memory/atomic_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/memory/shared_ptr_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/memory/unique_ptr_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/meta/move_test.cc
)

add_definitions(-DINTERNAL_EXPERIMENTAL)
add_definitions(-DFIREBASE_INTEGRATION_TEST)
#add_definitions(-DFIREBASE_TESTING)
set(FIREBASE_INTEGRATION_TEST_DESKTOP_SRCS
#  ${FIREBASE_CPP_SDK_DIR}/app/tests/heartbeat_storage_desktop_test.cc
#  ${FIREBASE_CPP_SDK_DIR}/app/tests/heartbeat_controller_desktop_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/request_binary_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/request_json_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/request_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/response_binary_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/response_json_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/response_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/transport_mock_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/transport_mock.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/util_test.cc
  ${FIREBASE_CPP_SDK_DIR}/app/rest/tests/www_form_url_encoded_test.cc
)


if(IOS)
  set(FIREBASE_INTEGRATION_TEST_SRCS
    ${FIREBASE_INTEGRATION_TEST_PORTABLE_SRCS}
  )
elif(ANDROID)
  set(FIREBASE_INTEGRATION_TEST_SRCS
    ${FIREBASE_INTEGRATION_TEST_PORTABLE_SRCS}
    ${FIREBASE_CPP_SDK_DIR}/app/tests/jobject_reference_test.cc
    ${FIREBASE_CPP_SDK_DIR}/app/tests/util_android_test.cc
  )
else()  # DESKTOP
  set(FIREBASE_INTEGRATION_TEST_SRCS
    ${FIREBASE_INTEGRATION_TEST_DESKTOP_SRCS}
    ${FIREBASE_INTEGRATION_TEST_PORTABLE_SRCS}
  )
endif()

# The include directory for the testapp.
include_directories(src)
# The include directory for the C++ SDK root.
include_directories(${FIREBASE_CPP_SDK_DIR})
# The include directory for the C++ SDK root.
include_directories(${FIREBASE_CPP_SDK_DIR})
# OpenSSL include directories.
include_directories(${OPENSSL_INCLUDE_DIR})

# Integration test uses some features that require C++ 11, such as lambdas.
set (CMAKE_CXX_STANDARD 11)

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

  # Windows runtime mode, either MD or MT depending on whether you are using
  # /MD or /MT. For more information see:
  # https://msdn.microsoft.com/en-us/library/2kzt1wy3.aspx
  set(MSVC_RUNTIME_MODE MD)

  # Platform abstraction layer for the desktop integration test.
  set(FIREBASE_APP_FRAMEWORK_DESKTOP_SRCS
    src/desktop/desktop_app_framework.cc
  )

  set(integration_test_target_name "integration_test")
  add_executable(${integration_test_target_name}
    ${FIREBASE_APP_FRAMEWORK_SRCS}
    ${FIREBASE_APP_FRAMEWORK_DESKTOP_SRCS}
    ${FIREBASE_TEST_FRAMEWORK_SRCS}
    ${FIREBASE_INTEGRATION_TEST_SRCS}
  )

  # Set a preprocessor define so that tests can distinguish between tests for
  # the desktop platforms (e.g. Windows, macOS, or Linux) and mobile platforms
  # (e.g. Android, iOS).
  target_compile_definitions(${integration_test_target_name}
    PRIVATE
    FIREBASE_TESTS_TARGET_DESKTOP
  )

  if(APPLE)
    set(ADDITIONAL_LIBS
      gssapi_krb5
      pthread
      "-framework CoreFoundation"
      "-framework Foundation"
      "-framework GSS"
      "-framework Security"
      "-framework SystemConfiguration"
    )
  elseif(MSVC)
    set(ADDITIONAL_LIBS advapi32 ws2_32 crypt32)
  else()
    set(ADDITIONAL_LIBS pthread)
  endif()

  # If a config file is present, copy it into the binary location so that it's
  # possible to create the default Firebase app.
  set(FOUND_JSON_FILE FALSE)
  foreach(config "google-services-desktop.json" "google-services.json")
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/${config}")
      add_custom_command(
        TARGET ${integration_test_target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_CURRENT_LIST_DIR}/${config}" $<TARGET_FILE_DIR:${integration_test_target_name}>)
      set(FOUND_JSON_FILE TRUE)
      break()
    endif()
  endforeach()
  if(NOT FOUND_JSON_FILE)
    message(WARNING "Failed to find either google-services-desktop.json or google-services.json. See the readme.md for more information.")
  endif()
endif()

# Don't include other Firebase libraries, only Firebase App
option(FIREBASE_INCLUDE_LIBRARY_DEFAULT "" OFF)

option(FIREBASE_CPP_BUILD_TESTS "" ON)
set(CURL_STATICLIB ON CACHE BOOL "")

add_subdirectory(${FIREBASE_CPP_SDK_DIR} bin/ EXCLUDE_FROM_ALL)

get_directory_property(ZLIB_SOURCE_DIR DIRECTORY ${FIREBASE_CPP_SDK_DIR} DEFINITION ZLIB_SOURCE_DIR)
get_directory_property(ZLIB_BINARY_DIR DIRECTORY ${FIREBASE_CPP_SDK_DIR} DEFINITION ZLIB_BINARY_DIR)
get_directory_property(FLATBUFFERS_SOURCE_DIR DIRECTORY ${FIREBASE_CPP_SDK_DIR} DEFINITION FLATBUFFERS_SOURCE_DIR)
get_directory_property(FIREBASE_GEN_FILE_DIR DIRECTORY ${FIREBASE_CPP_SDK_DIR} DEFINITION FIREBASE_GEN_FILE_DIR)
get_directory_property(GOOGLETEST_SOURCE_DIR DIRECTORY ${FIREBASE_CPP_SDK_DIR} DEFINITION GOOGLETEST_SOURCE_DIR)

# Additional include paths populated by top-level SDK CMakeLists
if(NOT ANDROID AND NOT IOS)
  # Turn absl-isms into gtest-isms
  target_compile_definitions(${integration_test_target_name}
    PUBLIC
      "-DCHECK=ASSERT_TRUE"
      "-DCHECK_EQ=ASSERT_EQ"
  )
  target_include_directories(${integration_test_target_name}
    PUBLIC
      ${FLATBUFFERS_SOURCE_DIR}/include
      ${FIREBASE_GEN_FILE_DIR}
      ${ZLIB_SOURCE_DIR}/..
  )
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} sample_resource_lib)
  if(LINUX)
    pkg_check_modules(LIBSECRET libsecret-1)
    if(NOT LIBSECRET_FOUND)
      message(FATAL_ERROR "Unable to find libsecret, which is needed by \
                           Firebase.  It can be installed on supported \
                           systems via: \
                           apt-get install libsecret-1-dev")
    endif()
    target_include_directories(${integration_test_target_name}
      PUBLIC
       ${LIBSECRET_INCLUDE_DIRS}
    )
  endif()
endif()


# gtest include directories.
target_include_directories(${integration_test_target_name}
  PRIVATE ${GOOGLETEST_SOURCE_DIR}/googletest
  PRIVATE ${GOOGLETEST_SOURCE_DIR}/googlemock
  PUBLIC ${GOOGLETEST_SOURCE_DIR}/googletest/include
  PUBLIC ${GOOGLETEST_SOURCE_DIR}/googlemock/include)

# Add the Firebase libraries to the target using the function from the SDK.
# Note that firebase_app needs to be last in the list.
set(firebase_libs firebase_app firebase_rest_lib firebase_testing flatbuffers libcurl)
target_link_libraries(${integration_test_target_name} ${firebase_libs} gtest gmock ${ADDITIONAL_LIBS})
