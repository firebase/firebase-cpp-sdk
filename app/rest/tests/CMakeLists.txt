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

include(binary_to_array)

set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-generated"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
# Because of a bug in the version of Flatbuffers we are pinned to,
# additional flags need to be set.
set(FLATC_ARGS "${FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS}")
build_flatbuffers("${CMAKE_CURRENT_LIST_DIR}/testdata/sample.fbs"
                  ""
                  "app_rest_generated_includes"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/app/rest"
                  ""
                  "")

binary_to_array("sample_resource"
    "${CMAKE_CURRENT_LIST_DIR}/testdata/sample.fbs"
    "firebase::rest"
    "${FIREBASE_GEN_FILE_DIR}/app/rest")

add_library(sample_resource_lib STATIC
  ${sample_resource_source}
  ${sample_resource_header}
  ${FIREBASE_GEN_FILE_DIR}/app/rest/sample_generated.h
)

target_include_directories(sample_resource_lib
  PUBLIC
    ${FLATBUFFERS_SOURCE_DIR}/include
)

firebase_cpp_cc_test(firebase_app_rest_request_test
  SOURCES
    request_test.h
    request_test.cc
  DEPENDS
    firebase_rest_lib
)

firebase_cpp_cc_test(firebase_app_rest_request_binary_test
  SOURCES
    request_binary_test.cc
  DEPENDS
    firebase_rest_lib
)

firebase_cpp_cc_test(firebase_app_rest_request_json_test
  SOURCES
    ../request_json.h
    request_json_test.cc
  DEPENDS
    firebase_app
    firebase_rest_lib
    sample_resource_lib
)

firebase_cpp_cc_test(firebase_app_rest_response_test
  SOURCES
    response_test.cc
  DEPENDS
    firebase_rest_lib
)

firebase_cpp_cc_test(firebase_app_rest_response_binary_test
  SOURCES
    response_binary_test.cc
  DEPENDS
    firebase_rest_lib
)

firebase_cpp_cc_test(firebase_app_rest_response_json_test
  SOURCES
    response_json_test.cc
  DEPENDS
    firebase_rest_lib
    sample_resource_lib
)

firebase_cpp_cc_test(firebase_app_rest_util_test
  SOURCES
    util_test.cc
  DEPENDS
    firebase_rest_lib
)

firebase_cpp_cc_test(firebase_app_rest_www_form_url_encoded_test
  SOURCES
    www_form_url_encoded_test.cc
  DEPENDS
    firebase_rest_lib
)

firebase_cpp_cc_test(firebase_app_rest_transport_mock_test
  SOURCES
    transport_mock_test.cc
    ../transport_mock.h
    ../transport_mock.cc
  INCLUDES
    ${FLATBUFFERS_SOURCE_DIR}/include
  DEPENDS
    firebase_app
    firebase_rest_lib
    firebase_testing
)

#[[

# google3 Dependency: FLAGS_test_tmpdir, CHECK(), CHECK_EQ
firebase_cpp_cc_test(firebase_app_rest_request_file_test
  SOURCES
    request_file_test.cc
  DEPENDS
    firebase_rest_lib
)

# google3 Dependency: absl/strings/escaping (absl::CEscape)
firebase_cpp_cc_test(firebase_app_rest_gzipheader_unittest
  SOURCES
    gzipheader_unittest.cc
  DEPENDS
    firebase_rest_lib
)

# google3 Dependency: absl/strings/escaping (absl::CEscape)
firebase_cpp_cc_test(firebase_app_rest_zlibwrapper_unittest
  SOURCES
    zlibwrapper_unittest.cc
  DEPENDS
    firebase_rest_lib
)

# google3 Dependency: net/.../http2server, net/util/ports.h (net_util::PickUnusedPort())
firebase_cpp_cc_test(firebase_app_rest_transport_curl_test
  SOURCES
    transport_curl_test.cc
  DEPENDS
    firebase_rest_lib
)

]]
