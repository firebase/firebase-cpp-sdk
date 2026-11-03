# Copyright 2020 Google
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

cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
project(integration_test C CXX)

# Find the firebase Libraries.
if (NOT DEFINED FIREBASE_CPP_SDK_DIR)
  if(DEFINED ENV{FIREBASE_CPP_SDK_DIR})
    set(FIREBASE_CPP_SDK_DIR $ENV{FIREBASE_CPP_SDK_DIR})
  else()
    get_filename_component(
      FIREBASE_CPP_SDK_DIR
      "${CMAKE_CURRENT_LIST_DIR}/../.."
      ABSOLUTE
    )
  endif()
endif()

if (NOT EXISTS "${FIREBASE_CPP_SDK_DIR}")
  message(FATAL_ERROR
          "Firebase C++ SDK was not found in ${FIREBASE_CPP_SDK_DIR}.")
endif()

# Include Google Test and the test framework.
add_subdirectory(
  "${CMAKE_CURRENT_LIST_DIR}/../../testing/test_framework"
  "${CMAKE_CURRENT_BINARY_DIR}/firebase_test_framework"
)

add_subdirectory("${FIREBASE_CPP_SDK_DIR}" bin/ EXCLUDE_FROM_ALL)

# Integrate Firebase App and Functions Libraries.
set(firebase_libs
  firebase_functions
  firebase_app_check
  firebase_auth
  firebase_app
  firebase_test_framework
)

firebase_test_framework_exe(
  integration_test
  SRCS "${CMAKE_CURRENT_LIST_DIR}/src/integration_test.cc"
  LIBS ${firebase_libs}
)
