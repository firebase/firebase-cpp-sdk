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

if("${FIREBASE_EXTERNAL_PLATFORM}" STREQUAL "DESKTOP" AND WIN32)
  set(_firebase_leveldb_patch_cmd
    git checkout -- . && git apply "${CMAKE_CURRENT_LIST_DIR}/../../scripts/git/patches/leveldb/0001-leveldb-1.23-windows-paths.patch")
else()
  set(_firebase_leveldb_patch_cmd "")
endif()

# This version must be kept in sync with the version in firestore.patch.txt.
# If this version ever changes then make sure to update the version in
# firestore.patch.txt accordingly.
# 99b3c03b3284f5886f9ef9a4ef703d57373e61be is tag 1.23.
set(_firebase_leveldb_version "99b3c03b3284f5886f9ef9a4ef703d57373e61be")

ExternalProject_Add(
  leveldb

  DOWNLOAD_DIR "${FIREBASE_DOWNLOAD_DIR}"
  GIT_REPOSITORY https://github.com/google/leveldb.git
  GIT_TAG "${_firebase_leveldb_version}"

  PREFIX "${PROJECT_BINARY_DIR}"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
  HTTP_HEADER "${EXTERNAL_PROJECT_HTTP_HEADER}"
  PATCH_COMMAND ${_firebase_leveldb_patch_cmd}
)

unset(_firebase_leveldb_patch_cmd)
unset(_firebase_leveldb_version)
