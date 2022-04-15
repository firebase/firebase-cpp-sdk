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

include(ExternalProject)
include(FindPythonInterp)

if(TARGET firestore)
  return()
endif()

ExternalProject_Add(
  firestore

  DOWNLOAD_DIR ${FIREBASE_DOWNLOAD_DIR}
  GIT_REPOSITORY https://github.com/firebase/firebase-ios-sdk
  GIT_TAG 0ed860a0360735c28a26aad9cbe899695009a236
  GIT_SHALLOW ON

  PREFIX ${PROJECT_BINARY_DIR}

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND ""
  PATCH_COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/firestore_patch.py --leveldb-version-from ${CMAKE_CURRENT_LIST_DIR}/leveldb.cmake
  HTTP_HEADER "${EXTERNAL_PROJECT_HTTP_HEADER}"
)
