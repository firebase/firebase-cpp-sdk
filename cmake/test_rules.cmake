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

include(CMakeParseArguments)

# cc_test(
#   target
#   SOURCES sources...
#   DEPENDS libraries...
# )
#
# Defines a new test executable target with the given target name, sources, and
# dependencies.  Implicitly adds DEPENDS on gtest and gtest_main.
function(cc_test name)
  set(multi DEPENDS SOURCES INCLUDES DEFINES)
  # Parse the arguments into cc_test_SOURCES and cc_test_DEPENDS.
  cmake_parse_arguments(cc_test "" "" "${multi}" ${ARGN})

  list(APPEND cc_test_DEPENDS gmock gtest gtest_main)

  add_executable(${name} ${cc_test_SOURCES})
  add_test(${name} ${name})
  target_include_directories(${name}
    PRIVATE
      ${FIREBASE_SOURCE_DIR}
      ${cc_test_INCLUDES}
  )
  target_link_libraries(${name} PRIVATE ${cc_test_DEPENDS})
  target_compile_definitions(${name}
    PRIVATE
      -DINTERNAL_EXPERIMENTAL=1
      ${cc_test_DEFINES}
  )
endfunction()