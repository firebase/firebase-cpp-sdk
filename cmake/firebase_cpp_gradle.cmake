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

# If the expected file does not already exist, defines a custom command to use
# the Gradle Wrapper in the root directory to invoke the given build rule.
#
# Args:
#   GRADLE_TARGET: The gradle target that will be built, if needed.
#   EXPECTED_FILE: The expected output from the gradle target.
function(firebase_cpp_gradle GRADLE_TARGET EXPECTED_FILE)
  if(NOT EXISTS ${EXPECTED_FILE})
    if(WIN32)
      set(gradle_command "gradlew.bat")
    else()
      set(gradle_command "./gradlew")
    endif()

    # Run gradle to generated the expected file
    add_custom_command(
      COMMAND ${gradle_command} ${GRADLE_TARGET}
      OUTPUT ${EXPECTED_FILE}
      WORKING_DIRECTORY ${FIREBASE_SOURCE_DIR}
    )
  endif()
endfunction()

# Defines a variable for generating the proguard file for a given library.
# The variable is FIREBASE_CPP_${LIBRARY_NAME}_PROGUARD, all in upper case.
#
# Args:
#   LIBRARY_NAME: The name of the library to define the proguard file for.
function(firebase_cpp_proguard_file LIBRARY_NAME)
  string(TOUPPER "${LIBRARY_NAME}" upper_name)
  set(proguard_var "FIREBASE_CPP_${upper_name}_PROGUARD")
  set(${proguard_var}
      "${FIREBASE_SOURCE_DIR}/${LIBRARY_NAME}/build/${CMAKE_BUILD_TYPE}/${LIBRARY_NAME}.pro"
      CACHE FILEPATH "Proguard file for ${LIBRARY_NAME}" FORCE)

  firebase_cpp_gradle(":${LIBRARY_NAME}:externalNativeBuildRelease"
                      "${${proguard_var}}")

  add_custom_target(
    "${proguard_var}"
    DEPENDS ${${proguard_var}}
  )
endfunction()