# Copyright 2024 Google LLC
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

if(NOT ANDROID AND NOT IOS)
  firebase_cpp_cc_test(
    firebase_functions_test
    SOURCES
      functions_test.cc
      ${FIREBASE_CPP_SDK_ROOT_DIR}/app/rest/transport_mock.cc
    DEPENDS
      firebase_app_for_testing
      firebase_functions
      firebase_testing
    DEFINES
      -DFIREBASE_TESTING
  )
endif()
