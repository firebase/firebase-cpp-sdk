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

# DO NOT MERGE: temporary pin to firebase/firebase-ios-sdk#17051 head
# (Windows CryptoAPI TLS + embedded roots.pem removal) with firestore.patch.txt
# disabled, to verify the full firebase-cpp-sdk Desktop matrix against it.
set(version e0488344f7d4ae66cfe3415e14f9cc35044b9b52)

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
    GIT_CONFIG "core.autocrlf=input"
    GIT_SHALLOW "ON"

    PREFIX ${PROJECT_BINARY_DIR}

    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    HTTP_HEADER "${EXTERNAL_PROJECT_HTTP_HEADER}"
    )
endfunction()

if((NOT FIRESTORE_DEP_SOURCE) OR (FIRESTORE_DEP_SOURCE STREQUAL "RELEASED"))
  # Get from released dependency by default
  GetReleasedDep()
else()
  if(FIRESTORE_DEP_SOURCE STREQUAL "TIP")
    GetTag("origin/main")
  else()
    GetTag(${FIRESTORE_DEP_SOURCE})
  endif()
endif()

