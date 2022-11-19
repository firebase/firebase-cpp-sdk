# Copyright 2022 Google LLC
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

# Invokes cmake's built-in execute_process() function, but also performs some
# logging and fails cmake if the process completes with a non-zero exit code.
#
# Arguments:
#   COMMAND
#     The command to run; it is forwarded verbatim to execute_process().
#   WORKING_DIRECTORY
#     The directory to set as the current working directory of the child process.
#   DESCRIPTION
#     (optional) A description to include in log messages about the command.
#     Example: "run the foo output through the bar filter"
function(execute_process_ex)
  # TODO: Upgrade the call of cmake_parse_arguments() to the PARSE_ARGV form
  # once the Android build upgrades its cmake version from 3.6 to 3.7+.
  cmake_parse_arguments(
    ARG
    "" # zero-value arguments
    "DESCRIPTION;WORKING_DIRECTORY" # single-value arguments
    "COMMAND" # multi-value arguments
    ${ARGN}
  )

  # Validate the arguments
  list(LENGTH ARG_COMMAND ARG_COMMAND_LENGTH)
  if(ARG_COMMAND_LENGTH EQUAL 0)
    message(
      FATAL_ERROR
      "COMMAND must be specified to execute_process_ex() "
      "and must have at least one value (DESCRIPTION=${ARG_DESCRIPTION})"
    )
  endif()
  if(NOT ("${ARG_UNPARSED_ARGUMENTS}" STREQUAL ""))
    message(
      FATAL_ERROR
      "Unexpected arguments to execute_process_ex(): ${ARG_UNPARSED_ARGUMENTS}"
    )
  endif()

  # Log that the process is about to be executed.
  set(ARG_COMMAND_STR "")
  foreach(ARG_COMMAND_STR_COMPONENT ${ARG_COMMAND})
    string(APPEND ARG_COMMAND_STR " ${ARG_COMMAND_STR_COMPONENT}")
  endforeach()
  string(SUBSTRING "${ARG_COMMAND_STR}" 1, -1 ARG_COMMAND_STR)
  if("${ARG_DESCRIPTION}" STREQUAL "")
    message(STATUS "Command starting: ${ARG_COMMAND_STR}")
  else()
    message(STATUS "Command starting (${ARG_DESCRIPTION}): ${ARG_COMMAND_STR}")
  endif()

  # Build up the arguments to specify to execute_process().
  set(
    EXECUTE_PROCESS_ARGS
    COMMAND
      ${ARG_COMMAND}
    RESULT_VARIABLE
      EXECUTE_PROCESS_RESULT
  )
  if(NOT ("${ARG_WORKING_DIRECTORY}" STREQUAL ""))
    list(
      APPEND
      EXECUTE_PROCESS_ARGS
      WORKING_DIRECTORY
      "${ARG_WORKING_DIRECTORY}"
    )
  endif()

  # Execute the process
  execute_process(${EXECUTE_PROCESS_ARGS})

  # Verify that the command completed successfully.
  if(NOT (EXECUTE_PROCESS_RESULT EQUAL 0))
    message(
      FATAL_ERROR
      "Command completed with non-zero exit code ${EXECUTE_PROCESS_RESULT} "
      "(DESCRIPTION=${ARG_DESCRIPTION}): ${ARG_COMMAND_STR}"
    )
  endif()

  # Log the success of the command.
  if("${ARG_DESCRIPTION}" STREQUAL "")
    message(STATUS "Command completed successfully: ${ARG_COMMAND_STR}")
  else()
    message(
      STATUS
      "Command completed successfully (${ARG_DESCRIPTION}): ${ARG_COMMAND_STR}"
    )
  endif()

endfunction(execute_process_ex)
