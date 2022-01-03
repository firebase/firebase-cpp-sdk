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

# Download GoogleTest from GitHub as an external project.
# Pin to 1.11.0 because we touch internal GoogleTest structures that could change in the future.

# This CMake file is taken from:
# https://github.com/google/googletest/blob/master/googletest/README.md#incorporating-into-an-existing-cmake-project

cmake_minimum_required(VERSION 2.8.2)

project(googletest-download NONE)

include(ExternalProject)
ExternalProject_Add(googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           "release-1.11.0"
  SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/src"
  BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/build"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)
