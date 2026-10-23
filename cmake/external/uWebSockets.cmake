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

# v0.14.8, the last release with the C++ client (uWS::Hub::connect).
set(uwebsockets_commit_tag 6d80b42e56f03295816f11371e6713690c87efdf)

ExternalProject_Add(
  uWebSockets

  DOWNLOAD_COMMAND 
    COMMAND git init uWebSockets
    COMMAND cd uWebSockets && git fetch --depth=1 https://github.com/uNetworking/uWebSockets.git ${uwebsockets_commit_tag} && git reset --hard FETCH_HEAD

  PREFIX ${PROJECT_BINARY_DIR}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND ""
  HTTP_HEADER "${EXTERNAL_PROJECT_HTTP_HEADER}"
)
