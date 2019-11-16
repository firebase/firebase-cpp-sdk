# Copyright 2019 Google
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

if(TARGET nanopb OR NOT DOWNLOAD_NANOPB)
  return()
endif()

ExternalProject_Add(
  nanopb

  DOWNLOAD_DIR ${FIREBASE_DOWNLOAD_DIR}
  URL https://github.com/nanopb/nanopb/archive/0.3.9.2.tar.gz
  URL_HASH SHA256=b8dd5cb0d184d424ddfea13ddee3f7b0920354334cbb44df434d55e5f0086b12

  PREFIX ${PROJECT_BINARY_DIR}

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND ""
)