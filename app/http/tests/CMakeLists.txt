# Copyright 2026 Google LLC
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

firebase_cpp_cc_test(firebase_app_http_common_test
  SOURCES
    http_transport_common_test.cc
  DEPENDS
    firebase_http_transport
)

firebase_cpp_cc_test(firebase_app_http_curl_test
  SOURCES
    http_transport_curl_test.cc
  DEPENDS
    firebase_http_transport
)

if(WIN32)
  target_link_libraries(firebase_app_http_curl_test PRIVATE ws2_32)
endif()
