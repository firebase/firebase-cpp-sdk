# Copyright 2017 Google
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

if(TARGET leveldb)
  return()
endif()

set(patch_file 
  ${CMAKE_CURRENT_LIST_DIR}/../../scripts/git/patches/leveldb/0001-leveldb-1.23-windows-paths.patch)

# This version must be kept in sync with the version in firestore.patch.txt.
# If this version ever changes then make sure to update the version in
# firestore.patch.txt accordingly.
set(version 1.23)

ExternalProject_Add(
  leveldb

  DOWNLOAD_DIR ${FIREBASE_DOWNLOAD_DIR}
  GIT_REPOSITORY https://github.com/google/leveldb.git
  GIT_TAG "${version}"

  PREFIX ${PROJECT_BINARY_DIR}

  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
  HTTP_HEADER "${EXTERNAL_PROJECT_HTTP_HEADER}"
  PATCH_COMMAND git apply ${patch_file} && git gc --aggressive
)
