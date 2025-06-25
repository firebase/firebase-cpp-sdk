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


if(WIN32)
  set(ANALYTICS_TEST_PLATFORM_DEPS Crypt32.lib)
else()
  set(ANALYTICS_TEST_PLATFORM_DEPS "")
endif()

firebase_cpp_cc_test(
  firebase_analytics_test
  SOURCES
    analytics_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_analytics
    firebase_testing
    ${ANALYTICS_TEST_PLATFORM_DEPS}
)

firebase_cpp_cc_test_on_ios(
  firebase_analytics_test
  HOST
    firebase_app_for_testing_ios
  SOURCES
    ${FIREBASE_SOURCE_DIR}/analytics/tests/analytics_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_analytics
    firebase_testing
    "-lsqlite3"
  CUSTOM_FRAMEWORKS
    StoreKit
)
