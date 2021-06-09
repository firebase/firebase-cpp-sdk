# Copyright 2018 Google LLC
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

if(TARGET boringssl OR NOT DOWNLOAD_BORINGSSL)
  return()
endif()

set(patch_file 
  ../../../../scripts/git/patches/boringssl/0001-disable-C4255-converting-empty-params-to-void.patch)

ExternalProject_Add(
  boringssl

  GIT_REPOSITORY https://github.com/google/boringssl/
  GIT_TAG 83da28a68f32023fd3b95a8ae94991a07b1f6c62
  PATCH_COMMAND git apply ${patch_file}
  PREFIX ${PROJECT_BINARY_DIR}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)
