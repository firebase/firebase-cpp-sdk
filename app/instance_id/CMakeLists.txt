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

# CMake file for the firebase_instance_id_desktop_impl library

cmake_minimum_required (VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

project(firebase_instance_id_desktop_impl NONE)
enable_language(C)
enable_language(CXX)

# Build the iid_data_generated.h header from the flatbuffer schema file.
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-generated"
    "--gen-object-api"
    "--cpp-ptr-type" "firebase::UniquePtr")
# Because of a bug in the version of Flatbuffers we are pinned to,
# additional flags need to be set.
set(FLATC_ARGS "${FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS}")
build_flatbuffers("${CMAKE_CURRENT_LIST_DIR}/iid_data.fbs"
                  ""
                  "iid_data_generated_includes"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/app/instance_id"
                  ""
                  "")

set(iid_impl_SRCS
    instance_id_desktop_impl.cc
    instance_id_desktop_impl.h
    ${FIREBASE_GEN_FILE_DIR}/app/instance_id/iid_data_generated.h)

add_library(firebase_instance_id_desktop_impl STATIC
    ${iid_impl_SRCS})

target_include_directories(firebase_instance_id_desktop_impl
  PUBLIC
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${FIREBASE_GEN_FILE_DIR}
)
target_link_libraries(firebase_instance_id_desktop_impl
  PUBLIC
    firebase_app
  PRIVATE
    flatbuffers
    firebase_rest_lib
)
target_compile_definitions(firebase_instance_id_desktop_impl
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
)

cpp_pack_library(firebase_instance_id_desktop_impl "deps/app")
