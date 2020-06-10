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

# CMake file for the firebase_auth library

# Generate the resource files from the flatbuffer schema files.
binary_to_array("request_resource"
    "${CMAKE_CURRENT_LIST_DIR}/src/desktop/rpcs/request.fbs"
    "firebase::auth"
    "${FIREBASE_GEN_FILE_DIR}/auth")
binary_to_array("response_resource"
    "${CMAKE_CURRENT_LIST_DIR}/src/desktop/rpcs/response.fbs"
    "firebase::auth"
    "${FIREBASE_GEN_FILE_DIR}/auth")
binary_to_array("user_data_resource"
    "${CMAKE_CURRENT_LIST_DIR}/src/desktop/user_data.fbs"
    "firebase::auth"
    "${FIREBASE_GEN_FILE_DIR}/auth")

# Build the generated header from the flatbuffer schema files.
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
set(flatbuffer_schemas
    ${CMAKE_CURRENT_LIST_DIR}/src/desktop/rpcs/request.fbs
    ${CMAKE_CURRENT_LIST_DIR}/src/desktop/rpcs/response.fbs
    ${CMAKE_CURRENT_LIST_DIR}/src/desktop/user_data.fbs)
# Because of a bug in the version of Flatbuffers we are pinned to,
# additional flags need to be set.
set(FLATC_ARGS "${FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS}")
build_flatbuffers("${flatbuffer_schemas}"
                  ""
                  "generate_request_fbs"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/auth"
                  ""
                  "")

# Common source files used by all platforms
set(common_SRCS
    src/auth.cc
    src/common.cc
    src/common.h
    src/data.h
    src/user.cc)

# Define the resource build needed for Android
firebase_cpp_gradle(":auth:auth_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/auth_resources/build/dexed.jar")
binary_to_array("auth_resources"
    "${CMAKE_CURRENT_LIST_DIR}/auth_resources/build/dexed.jar"
    "firebase_auth"
    "${FIREBASE_GEN_FILE_DIR}/auth")

# Source files used by the Android implementation.
set(android_SRCS
    ${auth_resources_source}
    src/android/auth_android.cc
    src/android/common_android.cc
    src/android/common_android.h
    src/android/credential_android.cc
    src/android/user_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/auth_ios.mm
    src/ios/common_ios.h
    src/ios/credential_ios.mm
    src/ios/user_ios.mm)

# Source files used by the desktop implementation.
set(desktop_SRCS
    ${request_resource_source}
    ${request_resource_header}
    ${response_resource_source}
    ${response_resource_header}
    ${user_data_resource_source}
    ${user_data_resource_header}
    ${FIREBASE_GEN_FILE_DIR}/auth/request_generated.h
    ${FIREBASE_GEN_FILE_DIR}/auth/response_generated.h
    ${FIREBASE_GEN_FILE_DIR}/auth/user_data_generated.h
    src/desktop/auth_constants.cc
    src/desktop/auth_credential.cc
    src/desktop/auth_desktop.cc
    src/desktop/auth_providers/email_auth_provider.cc
    src/desktop/auth_providers/facebook_auth_provider.cc
    src/desktop/auth_providers/federated_auth_provider.cc
    src/desktop/auth_providers/gamecenter_auth_provider.cc
    src/desktop/auth_providers/github_auth_provider.cc
    src/desktop/auth_providers/google_auth_provider.cc
    src/desktop/auth_providers/oauth_auth_provider.cc
    src/desktop/auth_providers/phone_auth_provider.cc
    src/desktop/auth_providers/playgames_auth_provider.cc
    src/desktop/auth_providers/twitter_auth_provider.cc
    src/desktop/auth_util.cc
    src/desktop/authentication_result.cc
    src/desktop/credential_desktop.cc
    src/desktop/credential_impl.cc
    src/desktop/credential_util.cc
    src/desktop/get_account_info_result.cc
    src/desktop/get_additional_user_info.cc
    src/desktop/set_account_info_result.cc
    src/desktop/sign_in_flow.cc
    src/desktop/user_desktop.cc
    src/desktop/user_view.cc
    src/desktop/rpcs/auth_request.cc
    src/desktop/rpcs/auth_response.cc
    src/desktop/rpcs/create_auth_uri_request.cc
    src/desktop/rpcs/delete_account_request.cc
    src/desktop/rpcs/error_codes.cc
    src/desktop/rpcs/get_account_info_request.cc
    src/desktop/rpcs/get_oob_confirmation_code_request.cc
    src/desktop/rpcs/reset_password_request.cc
    src/desktop/rpcs/secure_token_request.cc
    src/desktop/rpcs/set_account_info_request.cc
    src/desktop/rpcs/sign_up_new_user_request.cc
    src/desktop/rpcs/verify_assertion_request.cc
    src/desktop/rpcs/verify_custom_token_request.cc
    src/desktop/rpcs/verify_password_request.cc)

if(ANDROID)
  set(auth_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(auth_platform_SRCS
      "${ios_SRCS}")
else()
  set(auth_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS)
  set(additional_include_DIR)
  set(additional_link_LIB)
  set(additional_target_definitions)
else()
  set(additional_include_DIR
      ${FLATBUFFERS_SOURCE_DIR}/include)
  set(additional_link_LIB
      firebase_rest_lib)
endif()

add_library(firebase_auth STATIC
    ${common_SRCS}
    ${auth_platform_SRCS})

set_property(TARGET firebase_auth PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_auth
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_auth
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${additional_include_DIR}
)
target_compile_definitions(firebase_auth
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
  firebase_cpp_proguard_file(auth)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_auth
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  setup_pod_headers(
    firebase_auth
    POD_NAMES
      FirebaseCore
      FirebaseAuth
  )
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_auth "")
cpp_pack_public_headers()
