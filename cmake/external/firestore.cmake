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

if(TARGET firestore)
  return()
endif()

# If the format of the line below changes, then be sure to update
# https://github.com/firebase/firebase-cpp-sdk/blob/fd054fa016/.github/workflows/update-dependencies.yml#L81
set(version CocoaPods-10.7.0)

function(GetReleasedDep)
  message("Getting released firebase-ios-sdk @ ${version}")
  ExternalProject_Add(
    firestore

    DOWNLOAD_DIR ${FIREBASE_DOWNLOAD_DIR}
    URL https://github.com/firebase/firebase-ios-sdk/archive/${version}.tar.gz

    PREFIX ${PROJECT_BINARY_DIR}

    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    PATCH_COMMAND patch -Np1 -i ${CMAKE_CURRENT_LIST_DIR}/firestore.patch.txt
    HTTP_HEADER "${EXTERNAL_PROJECT_HTTP_HEADER}"
    )
endfunction()

function(GetTag t)
  message("Getting firebase-ios-sdk from ${t}")
  ExternalProject_Add(
    firestore

    DOWNLOAD_DIR ${FIREBASE_DOWNLOAD_DIR}
    GIT_REPOSITORY "https://github.com/firebase/firebase-ios-sdk.git"
    GIT_TAG ${t}
    GIT_SHALLOW "ON"

    PREFIX ${PROJECT_BINARY_DIR}

    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    PATCH_COMMAND patch -Np1 -i ${CMAKE_CURRENT_LIST_DIR}/firestore.patch.txt
    HTTP_HEADER "${EXTERNAL_PROJECT_HTTP_HEADER}"
    )
endfunction()

if((NOT FIRESTORE_DEP_SOURCE) OR (FIRESTORE_DEP_SOURCE STREQUAL "RELEASED"))
  # Get from released dependency by default
  GetReleasedDep()
else()
  if(FIRESTORE_DEP_SOURCE STREQUAL "TIP")
    GetTag("origin/master")
  else()
    GetTag(${FIRESTORE_DEP_SOURCE})
  endif()
endif()

