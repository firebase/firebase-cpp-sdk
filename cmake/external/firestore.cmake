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

# Pin to the first revision that includes these changes:
# https://github.com/firebase/firebase-ios-sdk/pull/5052
#
# These changes are required for the firebase-ios-sdk build to interoperate
# well with a wrapper build that also uses googletest. Once M67 iOS
# releases with that change, this should point to the Firestore release tag.
set(version a568a96f7e6cf26f74d3c04892d02c4a218d5566)

ExternalProject_Add(
  firestore

  DOWNLOAD_DIR ${FIREBASE_DOWNLOAD_DIR}
  DOWNLOAD_NAME firestore-${version}.tar.gz
  URL https://github.com/firebase/firebase-ios-sdk/archive/${version}.tar.gz

  PREFIX ${PROJECT_BINARY_DIR}

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND ""
)
