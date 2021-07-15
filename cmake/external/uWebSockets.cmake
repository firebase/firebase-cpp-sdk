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

include(ExternalProject)

if(TARGET uWebSockets OR NOT DOWNLOAD_UWEBSOCKETS)
  return()
endif()

set(uwebsockets_commit_tag 4d94401b9c98346f9afd838556fdc7dce30561eb)
set(patch_file 
  ${CMAKE_CURRENT_LIST_DIR}/../../scripts/git/patches/uWebSockets/0001-fix-want-write-crash.patch)

ExternalProject_Add(
  uWebSockets

  DOWNLOAD_COMMAND 
    COMMAND git init uWebSockets
    COMMAND cd uWebSockets && git fetch --depth=1 https://github.com/uNetworking/uWebSockets.git ${uwebsockets_commit_tag} && git reset --hard FETCH_HEAD

  PATCH_COMMAND git apply ${patch_file} && git gc --aggressive
  PREFIX ${PROJECT_BINARY_DIR}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND ""
)
