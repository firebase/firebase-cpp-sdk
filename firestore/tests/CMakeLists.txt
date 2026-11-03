# Copyright 2024 Google LLC
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

if(NOT ANDROID AND NOT IOS AND EXISTS "${FIRESTORE_SOURCE_DIR}/Firestore/core/src")
  firebase_cpp_cc_test(
    firebase_firestore_test
    SOURCES
      firestore_test.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/compiler_info.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/exception_common.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/field_path.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/futures.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/hard_assert_common.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/load_bundle_task_progress.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/set_options.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/settings.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/snapshot_metadata.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/transaction_options.cc
      ${CMAKE_CURRENT_LIST_DIR}/../src/common/util.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/api/settings.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/geo_point.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/model/field_path.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/timestamp.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/timestamp_internal.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/comparison.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/exception.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/executor_std.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/hard_assert.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/log_stdio.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/schedule.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/status.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/statusor.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/strerror.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/string_format.cc
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/src/util/task.cc
    INCLUDES
      ${CMAKE_CURRENT_LIST_DIR}/../src/include
      ${FIRESTORE_BINARY_DIR}
      ${FIRESTORE_BINARY_DIR}/Firestore/core/src/util
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/include
      ${FIRESTORE_SOURCE_DIR}/Firestore/Protos/nanopb
      ${FIRESTORE_SOURCE_DIR}
      ${FIRESTORE_BINARY_DIR}/external/src/abseil-cpp
      ${FIRESTORE_BINARY_DIR}/external/src/nanopb
      ${FIRESTORE_BINARY_DIR}/external/src/grpc/include
      ${FIRESTORE_BINARY_DIR}/external/src/protobuf/src
    DEFINES
      -DPB_FIELD_32BIT
      -DPB_ENABLE_MALLOC
      -DPB_NO_PACKED_STRUCTS
    DEPENDS
      absl_strings
      absl_time
      absl_raw_logging_internal
      absl_throw_delegate
      firebase_app
      firebase_testing
  )

  if(TARGET firebase_firestore_test AND NOT MSVC)
    target_compile_options(firebase_firestore_test PRIVATE -fexceptions)
  endif()
endif()
