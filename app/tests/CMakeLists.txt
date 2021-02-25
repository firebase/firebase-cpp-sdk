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

# Set up a library for create an app suitable for testing. Needs an empty
# source file, as not all compilers handle header only libraries with CMake.

file(WRITE ${CMAKE_BINARY_DIR}/empty.cc)
add_library(firebase_app_for_testing
  ${CMAKE_BINARY_DIR}/empty.cc
  ${FIREBASE_SOURCE_DIR}/app/tests/include/firebase/app_for_testing.h
)

target_include_directories(firebase_app_for_testing
  PUBLIC
    ${FIREBASE_SOURCE_DIR}/app/tests/include/firebase
)

if (ANDROID)
elseif (IOS)
  set(TEST_RUNNER_DIR "${FIREBASE_SOURCE_DIR}/app/src/tests/runner/ios")
  add_executable(firebase_app_for_testing_ios MACOSX_BUNDLE
    ${TEST_RUNNER_DIR}/FIRAppDelegate.m
    ${TEST_RUNNER_DIR}/FIRAppDelegate.h
    ${TEST_RUNNER_DIR}/FIRViewController.m
    ${TEST_RUNNER_DIR}/FIRViewController.h
    ${TEST_RUNNER_DIR}/main.m
    ${FIREBASE_SOURCE_DIR}/app/tests/include/firebase/app_for_testing.h
    ${FIREBASE_SOURCE_DIR}/app/src/fake/FIRApp.mm
    ${FIREBASE_SOURCE_DIR}/app/src/fake/FIROptions.mm
  )

  target_include_directories(firebase_app_for_testing_ios
    PUBLIC
      ${FIREBASE_SOURCE_DIR}/app/src/fake
    PRIVATE
      ${FIREBASE_SOURCE_DIR}
  )

  target_link_libraries(
    firebase_app_for_testing_ios
    PRIVATE
      "-framework UIKit"
      "-framework Foundation"
  )
  set_target_properties(
    firebase_app_for_testing_ios PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST
      ${TEST_RUNNER_DIR}/Info.plist
    RESOURCE
      ${TEST_RUNNER_DIR}/Info.plist
  )
else()
  set(rest_mocks_SRCS
      ${FIREBASE_CPP_SDK_ROOT_DIR}/app/rest/transport_mock.h
      ${FIREBASE_CPP_SDK_ROOT_DIR}/app/rest/transport_mock.cc)

  add_library(firebase_rest_mocks STATIC
      ${rest_mocks_SRCS})
  target_include_directories(firebase_rest_mocks
    PRIVATE
      ${FIREBASE_CPP_SDK_ROOT_DIR}
      ${FIREBASE_GEN_FILE_DIR}
  )
  target_link_libraries(firebase_rest_mocks
    PRIVATE
      firebase_rest_lib
      firebase_testing
  )
endif()

firebase_cpp_cc_test_on_ios(firebase_app_test
  SOURCES
    ${FIREBASE_SOURCE_DIR}/app/tests/app_test.cc
  HOST
    firebase_app_for_testing_ios
  DEPENDS
    firebase_app_for_testing
    firebase_app
    firebase_testing
    flatbuffers
  CUSTOM_FRAMEWORKS
    UIKit
)

firebase_cpp_cc_test(firebase_app_log_test
  SOURCES
    ${FIREBASE_SOURCE_DIR}/app/tests/log_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test_on_ios(firebase_app_log_test
  HOST
    firebase_app_for_testing_ios
  SOURCES
    ${FIREBASE_SOURCE_DIR}/app/tests/log_test.cc
  DEPENDS
    firebase_app
    firebase_app_for_testing
    firebase_testing
)

firebase_cpp_cc_test(firebase_app_logger_test
  SOURCES
    ${FIREBASE_SOURCE_DIR}/app/tests/logger_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_semaphore_test
  SOURCES
    ${FIREBASE_SOURCE_DIR}/app/tests/semaphore_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_assert_test
  SOURCES
    assert_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_assert_release_test
  SOURCES
    assert_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_optional_test
  SOURCES
    optional_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_cleanup_notifier_test
  SOURCES
    cleanup_notifier_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_cpp11_thread_test
  SOURCES
    thread_test.cc
  DEPENDS
    firebase_app
    gtest
)

firebase_cpp_cc_test(firebase_app_heartbeat_info_desktop_test
  SOURCES
    heartbeat_info_desktop_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_pthread_thread_test
  SOURCES
    thread_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_time_tests
  SOURCES
    time_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_scheduler_test
  SOURCES
    scheduler_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_path_test
  SOURCES
    path_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_locale_test
  SOURCES
    locale_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_callback_test
  SOURCES
    callback_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_reference_count_test
  SOURCES
    reference_count_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_util_test
  SOURCES
    util_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_uuid_test
  SOURCES
    uuid_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_variant_test
  SOURCES
    variant_test.cc
  DEPENDS
    firebase_app
)

add_library(flexbuffer_matcher
  flexbuffer_matcher.cc
)

target_include_directories(flexbuffer_matcher
  PRIVATE
    ${FIREBASE_SOURCE_DIR}
    ${FLATBUFFERS_SOURCE_DIR}/include
)

target_link_libraries(flexbuffer_matcher
  PRIVATE
    flatbuffers
    firebase_testing
    gmock
    gtest
)

firebase_cpp_cc_test(flexbuffer_matcher_test
  SOURCES
    flexbuffer_matcher_test.cc
  DEPENDS
    firebase_app
    firebase_testing
    flexbuffer_matcher
    flatbuffers
)

firebase_cpp_cc_test(firebase_app_variant_util_tests
  SOURCES
    variant_util_test.cc
  DEPENDS
    firebase_app
    firebase_testing
    flexbuffer_matcher
    flatbuffers
)

if (NOT IOS AND APPLE)
  add_library(firebase_app_secure_darwin_testlib
    ${FIREBASE_CPP_SDK_ROOT_DIR}/app/src/secure/user_secure_darwin_internal_testlib.mm
  )
  target_include_directories(firebase_app_secure_darwin_testlib
    PRIVATE
      ${FIREBASE_CPP_SDK_ROOT_DIR}
  )
  target_link_libraries(firebase_app_secure_darwin_testlib
    PUBLIC
      "-framework Foundation"
      "-framework Security"
  )
  set(platform_secure_testlib firebase_app_secure_darwin_testlib)
else()
  set(platform_secure_testlib)
endif()

firebase_cpp_cc_test(firebase_app_user_secure_manager_test
  SOURCES
    secure/user_secure_manager_test.cc
  DEPENDS
    firebase_app
    ${platform_secure_testlib}
  DEFINES
    -DUSER_SECURE_LOCAL_TEST
)

if(FIREBASE_FORCE_FAKE_SECURE_STORAGE)
  set(SECURE_STORAGE_DEFINES
      -DFORCE_FAKE_SECURE_STORAGE
  )
endif()

firebase_cpp_cc_test(firebase_app_user_secure_integration_test
  SOURCES
    secure/user_secure_integration_test.cc
    ${FIREBASE_CPP_SDK_ROOT_DIR}/app/src/secure/user_secure_fake_internal.cc
  DEPENDS
    firebase_app
    firebase_testing
    ${platform_secure_testlib}
  INCLUDES
    ${LIBSECRET_INCLUDE_DIRS}
  DEFINES
    -DUSER_SECURE_LOCAL_TEST
    ${SECURE_STORAGE_DEFINES}
)

firebase_cpp_cc_test(firebase_app_user_secure_internal_test
  SOURCES
    secure/user_secure_internal_test.cc
    ${FIREBASE_CPP_SDK_ROOT_DIR}/app/src/secure/user_secure_fake_internal.cc
  DEPENDS
    firebase_app
    firebase_testing
    ${platform_secure_testlib}
  INCLUDES
    ${LIBSECRET_INCLUDE_DIRS}
  DEFINES
    -DUSER_SECURE_LOCAL_TEST
    ${SECURE_STORAGE_DEFINES}
)

firebase_cpp_cc_test(firebase_app_memory_atomic_test
  SOURCES
    ${FIREBASE_CPP_SDK_ROOT_DIR}/app/memory/atomic_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_memory_shared_ptr_test
  SOURCES
    ${FIREBASE_CPP_SDK_ROOT_DIR}/app/memory/shared_ptr_test.cc
  DEPENDS
    firebase_app
)

firebase_cpp_cc_test(firebase_app_memory_unique_ptr_test
  SOURCES
    ${FIREBASE_CPP_SDK_ROOT_DIR}/app/memory/unique_ptr_test.cc
  DEPENDS
    firebase_app
)

#[[ google3 Dependencies

# google3 - FLAGS_test_srcdir
firebase_cpp_cc_test(firebase_app_google_services_test
  SOURCES
    google_services_test.cc
  INCLUDES
    ${FIREBASE_GEN_FILE_DIR}
  DEPENDS
    flatbuffers
)

# google3 - FLAGS_test_srcdir
firebase_cpp_cc_test(firebase_app_desktop_test
  SOURCES
    app_test.cc
  DEPENDS
    firebase_app
    firebase_testing
)

# google3 - openssl/base64.h
firebase_cpp_cc_test(firebase_app_base64_test
  SOURCES
    base64_openssh_test.cc
    base64_test.cc
  INCLUDES
    ${OPENSSL_INCLUDE_DIR}
  DEPENDS
    firebase_app
    ${OPENSSL_CRYPTO_LIBRARY}
)

# google3 - thread/fiber/fiber.h (thread::Fiber)
firebase_cpp_cc_test(firebase_app_future_playbillingclient_test
  SOURCES
    future_playbillingclient_test.cc
  DEPENDS
    firebase_app
)

# google3 - thread/fiber/fiber.h (thread::Fiber)
firebase_cpp_cc_test(firebase_app_future_test
  SOURCES
    future_test.cc
  DEPENDS
    firebase_app
)

# google3 - thread/fiber/fiber.h (thread::Fiber)
firebase_cpp_cc_test(firebase_app_future_manager_test
  SOURCES
    future_manager_test.cc
  DEPENDS
    firebase_app
)


# google3 Dependencies ]]

