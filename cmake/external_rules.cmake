# Copyright 2018 Google
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

# Executes a CMake build of the external dependencies, which will pull them
# as ExternalProjects, under an "external" subdirectory.
function(download_external_sources)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/external)

  # Setup cmake environment.
  # These commands are executed from within the currect context, which has set
  # variables for the target platform. We use "env -i" to clear these
  # variables, and manually keep the PATH to regular bash path.
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    # Windows doesn't have an 'env' command
    set(ENV_COMMAND "")
  else()
    set(firebase_command_line_path "$ENV{PATH}")
    set(ENV_COMMAND env -i PATH=${firebase_command_line_path})
  endif()

  if (IOS)
    set(external_platform IOS)
  elseif(ANDROID)
    set(external_platform ANDROID)
  else()
    set(external_platform DESKTOP)
  endif()
  
  execute_process(
    COMMAND
      ${ENV_COMMAND} cmake
      -DCMAKE_INSTALL_PREFIX=${FIREBASE_INSTALL_DIR}
      -DFIREBASE_DOWNLOAD_DIR=${FIREBASE_DOWNLOAD_DIR}
      -DFIREBASE_EXTERNAL_PLATFORM=${external_platform}
      ${PROJECT_SOURCE_DIR}/cmake/external
    OUTPUT_FILE ${PROJECT_BINARY_DIR}/external/output_cmake_config.txt
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/external
  )

  # Run downloads in parallel if we know how
  if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set(cmake_build_args -j)
  endif()

  execute_process(
    COMMAND ${ENV_COMMAND} cmake --build . -- ${cmake_build_args}
    OUTPUT_FILE ${PROJECT_BINARY_DIR}/external/output_cmake_build.txt
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/external
  )
endfunction()

# Populates directory variables for the given name, to the location that name
# would be in after a call to download_external_sources.
function(populate_external_source_vars NAME)
  set(${NAME}_SOURCE_DIR "${FIREBASE_BINARY_DIR}/external/src/${NAME}"
      PARENT_SCOPE)
  set(${NAME}_BINARY_DIR "${FIREBASE_BINARY_DIR}/external/src/${NAME}-build"
      PARENT_SCOPE)
endfunction()

# Adds the given library's location as a subdirectory that the caller uses.
function(add_external_subdirectory NAME)
  add_subdirectory(
    ${FIREBASE_BINARY_DIR}/external/src/${NAME}
    ${FIREBASE_BINARY_DIR}/external/src/${NAME}-build
    EXCLUDE_FROM_ALL
  )
endfunction()
