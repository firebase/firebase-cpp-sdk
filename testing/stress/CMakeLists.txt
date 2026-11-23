# Copyright 2026 Google LLC
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

if(NOT DESKTOP OR NOT FIREBASE_CPP_BUILD_TESTS)
  return()
endif()

if(FIREBASE_INCLUDE_AUTH AND FIREBASE_INCLUDE_APP_CHECK AND
   FIREBASE_INCLUDE_STORAGE AND FIREBASE_INCLUDE_DATABASE AND
   FIREBASE_INCLUDE_FUNCTIONS AND FIREBASE_INCLUDE_INSTALLATIONS AND
   FIREBASE_INCLUDE_REMOTE_CONFIG)
  set(stress_unit_DEPS
    firebase_app
    firebase_app_check
    firebase_auth
    firebase_database
    firebase_functions
    firebase_installations
    firebase_remote_config
    firebase_storage
    firebase_testing
  )
  set(stress_unit_DEFS "")
  if(FIREBASE_INCLUDE_FIRESTORE)
    list(APPEND stress_unit_DEPS firebase_firestore)
    list(APPEND stress_unit_DEFS -DFIREBASE_STRESS_WITH_FIRESTORE=1)
  endif()

  firebase_cpp_cc_test(
    firebase_multi_app_lifecycle_unit_test
    SOURCES
      multi_app_lifecycle_unit_test.cc
    DEPENDS
      ${stress_unit_DEPS}
    DEFINES
      ${stress_unit_DEFS}
  )
  set_tests_properties(firebase_multi_app_lifecycle_unit_test PROPERTIES
    LABELS "multi_app_stress"
  )
endif()

if(FIREBASE_INCLUDE_AUTH AND FIREBASE_INCLUDE_APP_CHECK AND
   FIREBASE_INCLUDE_DATABASE AND FIREBASE_INCLUDE_FIRESTORE AND
   FIREBASE_INCLUDE_STORAGE)
  add_executable(firebase_multi_app_emulator_stress_test
    multi_app_emulator_stress_test.cc
  )
  target_include_directories(firebase_multi_app_emulator_stress_test
    PRIVATE
      ${FIREBASE_SOURCE_DIR}
  )
  target_link_libraries(firebase_multi_app_emulator_stress_test
    PRIVATE
      firebase_app
      firebase_app_check
      firebase_auth
      firebase_database
      firebase_firestore
      firebase_storage
      firebase_testing
      gmock
      gtest
      gtest_main
  )
  target_compile_definitions(firebase_multi_app_emulator_stress_test
    PRIVATE
      -DINTERNAL_EXPERIMENTAL=1
  )
endif()
