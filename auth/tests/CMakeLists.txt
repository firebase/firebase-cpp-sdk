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

set(desktop_fakes_SRCS
    desktop/fakes.h
    desktop/fakes.cc)
set(desktop_test_util_SRCS
    desktop/test_utils.h
    desktop/test_utils.cc
    )
set(ios_frameworks
    FirebaseAuth
    )

add_library(firebase_auth_desktop_test_util STATIC
    ${desktop_fakes_SRCS}
    ${desktop_test_util_SRCS})

target_include_directories(firebase_auth_desktop_test_util
    PRIVATE
      ${FIREBASE_CPP_SDK_ROOT_DIR}
      ${FIREBASE_GEN_FILE_DIR}
)

target_link_libraries(firebase_auth_desktop_test_util
    PRIVATE
        firebase_auth
        firebase_testing
        gtest
        gmock
)

target_compile_definitions(firebase_auth_desktop_test_util
    PRIVATE
        -DINTERNAL_EXPERIMENTAL=1
)


if (NOT ANDROID AND NOT IOS)
  set(desktop_rpc_test_util_SRCS
    desktop/rpcs/test_util.h
    desktop/rpcs/test_util.cc)

  add_library(firebase_auth_desktop_rpc_test_util STATIC
      ${desktop_rpc_test_util_SRCS})

  target_include_directories(firebase_auth_desktop_rpc_test_util
      PRIVATE
        ${FLATBUFFERS_SOURCE_DIR}/include
        ${FIREBASE_CPP_SDK_ROOT_DIR}
        ${FIREBASE_GEN_FILE_DIR}
  )

  target_link_libraries(firebase_auth_desktop_rpc_test_util
      PRIVATE
          firebase_auth
  )
endif()


firebase_cpp_cc_test(
  firebase_auth_test
  SOURCES
    auth_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_rest_mocks
    firebase_auth
    firebase_testing
  DEFINES
    -DFIREBASE_WAIT_ASYNC_IN_TEST
)

firebase_cpp_cc_test_on_ios(
  firebase_auth_test
  HOST
    firebase_app_for_testing_ios
  SOURCES
    credential_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
  CUSTOM_FRAMEWORKS
    ${ios_frameworks}
)

firebase_cpp_cc_test(
  firebase_auth_credential_test
  SOURCES
    credential_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test_on_ios(
  firebase_auth_credential_test
  HOST
    firebase_app_for_testing_ios
  SOURCES
    credential_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
  CUSTOM_FRAMEWORKS
    ${ios_frameworks}
)

firebase_cpp_cc_test(
  firebase_auth_user_test
  SOURCES
    user_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_rest_mocks
    firebase_auth
    firebase_testing
  DEFINES
    -DFIREBASE_WAIT_ASYNC_IN_TEST
)

firebase_cpp_cc_test_on_ios(
  firebase_auth_user_test
  HOST
     firebase_app_for_testing_ios
  SOURCES
    user_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
  CUSTOM_FRAMEWORKS
    ${ios_frameworks}
)

firebase_cpp_cc_test(
  firebase_auth_desktop_test
  SOURCES
    desktop/auth_desktop_test.cc
  DEPENDS
    firebase_auth
    firebase_auth_desktop_test_util
    firebase_rest_lib
    firebase_rest_mocks
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_user_desktop_test
  SOURCES
    desktop/user_desktop_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_auth_desktop_test_util
    firebase_rest_lib
    firebase_rest_mocks
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_create_auth_uri_test
  SOURCES
    desktop/rpcs/create_auth_uri_test.cc
  DEPENDS
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_delete_account_test_test
  SOURCES
    desktop/rpcs/delete_account_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_get_account_info_test
  SOURCES
    desktop/rpcs/get_account_info_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_get_oob_confirmation_code_test
  SOURCES
    desktop/rpcs/get_oob_confirmation_code_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_reset_password_test
  SOURCES
    desktop/rpcs/reset_password_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_secure_token_test
  SOURCES
    desktop/rpcs/secure_token_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_set_account_info_test
  SOURCES
    desktop/rpcs/set_account_info_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_sign_up_new_user_test
  SOURCES
    desktop/rpcs/sign_up_new_user_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_verify_assertion_test
  SOURCES
    desktop/rpcs/verify_assertion_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_verify_custom_token_test
  SOURCES
    desktop/rpcs/verify_custom_token_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_auth_verify_password_test
  SOURCES
    desktop/rpcs/verify_password_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_auth
    firebase_testing
)
