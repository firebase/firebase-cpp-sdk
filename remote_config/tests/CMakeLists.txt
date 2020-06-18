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

# TODO: This test is currently Android-only and needs extra java deps.
# TODO: Work out how to make it work for desktop
#[[
firebase_cpp_cc_test(
  firebase_remote_config_test
  SOURCES
    remote_config_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_remote_config
    firebase_testing
)
]]

firebase_cpp_cc_test(
  firebase_remote_config_desktop_config_data_test
  SOURCES
    desktop/config_data_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_remote_config
    firebase_testing
)
