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

set(FUTURE_LIB_SRC_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")

# Defines a Future library for the given namespace
function(define_future_lib CPP_NAMESPACE)
  set(library_name "${CPP_NAMESPACE}_future")

  # Modify the public Future header with the new namespace
  file(READ
       ${FUTURE_LIB_SRC_DIR}/../app/src/include/firebase/future.h
       old_future_header)
  string(TOUPPER ${CPP_NAMESPACE} upper_namespace)
  set(new_header_guard "FUTURE_INCLUDE_${upper_namespace}_FUTURE_H_")
  string(REPLACE
         FIREBASE_APP_CLIENT_CPP_SRC_INCLUDE_FIREBASE_FUTURE_H_
         ${new_header_guard}
         future_header_guard_changed
         "${old_future_header}")
  string(REPLACE
         "namespace FIREBASE_NAMESPACE {"
         "namespace ${CPP_NAMESPACE} {"
         future_header_namespace_changed
         "${future_header_guard_changed}")
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/future_include/${CPP_NAMESPACE})
  set(generated_future_header_path
      "${PROJECT_BINARY_DIR}/future_include/${CPP_NAMESPACE}/future.h")
  #TODO Change to use a configure_file, will need to change future.h, and the
  # Firebase library to use it as well.
  file(WRITE
       ${generated_future_header_path}
       "${future_header_namespace_changed}")

  if(ANDROID OR DEFINED ANDROID_NDK)
    set(log_SRCS
        ${FUTURE_LIB_SRC_DIR}/../app/src/log.cc
        ${FUTURE_LIB_SRC_DIR}/../app/src/log_android.cc)
  elseif(IOS OR "${CMAKE_OSX_SYSROOT}" MATCHES "iphoneos")
    set(log_SRCS
        ${FUTURE_LIB_SRC_DIR}/../app/src/log.cc
        ${FUTURE_LIB_SRC_DIR}/../app/src/log_ios.mm)
  else()
    set(log_SRCS
        ${FUTURE_LIB_SRC_DIR}/../app/src/log.cc
        ${FUTURE_LIB_SRC_DIR}/../app/src/log_stdio.cc)
  endif()

  add_library("${library_name}" STATIC
      ${FUTURE_LIB_SRC_DIR}/../app/src/cleanup_notifier.cc
      ${FUTURE_LIB_SRC_DIR}/../app/src/future_manager.cc
      ${FUTURE_LIB_SRC_DIR}/../app/src/reference_counted_future_impl.cc
      ${generated_future_header_path}
      ${log_SRCS})

  target_compile_definitions("${library_name}"
    PRIVATE
      -DFIREBASE_NAMESPACE=${CPP_NAMESPACE}
  )
  if("${CPP_NAMESPACE}" STREQUAL "playbillingclient")
    target_compile_definitions("${library_name}"
      PRIVATE
        -DUSE_PLAYBILLING_FUTURE=1
    )
  endif()
  target_include_directories("${library_name}"
    PUBLIC
      ${PROJECT_BINARY_DIR}/future_include
    PRIVATE
      ${FUTURE_LIB_SRC_DIR}/..
      ${FUTURE_LIB_SRC_DIR}/../app/src/include
  )
endfunction()