# Copyright 2020 Google
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

if(TARGET firebase_ios_sdk OR NOT DOWNLOAD_FIREBASE_IOS_SDK)
  return()
endif()

cmake_minimum_required(VERSION 3.0)

set(SDK_VERSION "6.14.0")

# Build the download URL.
string(REPLACE "." "_" SDK_VERSION_PATH ${SDK_VERSION})
string(CONCAT URL
       "https://dl.google.com/firebase/sdk/ios/"
       ${SDK_VERSION_PATH}
       "/Firebase-"
       ${SDK_VERSION}
       ".zip")

# Configure local storage of download.
set(ZIP_FILENAME "./firebase_sdk_${SDK_VERSION}.zip")
set(OUTDIR "./firebase_ios_sdk/")

# Download as required.
if(FIREBASE_CPP_BUILD_TESTS)
  if(NOT EXISTS ${OUTDIR})
    if(NOT EXISTS ${ZIP_FILENAME})
      file(DOWNLOAD ${URL} ${ZIP_FILENAME}) 
    endif()
    execute_process(COMMAND mkdir ${OUTDIR})
    execute_process(COMMAND tar -xf ${ZIP_FILENAME} -C ${OUTDIR})
    execute_process(COMMAND rm ${ZIP_FILENAME})
  endif()
endif()