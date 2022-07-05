# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0 #
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(FIRESTORE_ABSL_VERSION "20211102.0")
set(FIRESTORE_ABSL_SHA256 "dcf71b9cba8dc0ca9940c4b316a0c796be8fab42b070bb6b7cab62b48f0e66c4")
set(FIRESTORE_ABSL_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/external/abseil-cpp")
set(FIRESTORE_ABSL_SRC_DIR "${FIRESTORE_ABSL_BASE_DIR}/src/abseil-cpp-${FIRESTORE_ABSL_VERSION}")

# Find a Python interpreter using the best available mechanism.
if(${CMAKE_VERSION} VERSION_LESS "3.12")
  include(FindPythonInterp)
  set(FIRESTORE_ABSL_PYTHON_EXECUTABLE "${PYTHON_EXECUTABLE}")
else()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(FIRESTORE_ABSL_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()

execute_process(
  COMMAND
    "${FIRESTORE_ABSL_PYTHON_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/abseil_setup.py"
    "${FIRESTORE_ABSL_VERSION}"
    "${FIRESTORE_ABSL_BASE_DIR}"
    "${FIRESTORE_ABSL_SHA256}"
  RESULT_VARIABLE
    FIRESTORE_ABSL_SETUP_RESULT
)
if(NOT FIRESTORE_ABSL_SETUP_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to setup abseil")
endif()

add_subdirectory("${FIRESTORE_ABSL_SRC_DIR}")
