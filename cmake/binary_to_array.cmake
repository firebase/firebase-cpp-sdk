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

# Defines a custom_command to generate source and header files using the
# binary_to_array script. Generated files are at NAME_(source/header).
#
# Args:
#   NAME: The name to use for the generated files.
#   INPUT: The input binary to embed into the source file.
#   CPP_NAMESPACE: The namespace to use in the generated files.
#   OUTPUT_DIRECTORY: Where the generated files should be written to.
function(binary_to_array NAME INPUT CPP_NAMESPACE OUTPUT_DIRECTORY)
  # Guarantee the output directory exists
  file(MAKE_DIRECTORY ${OUTPUT_DIRECTORY})

  # Define variables for the output source and header for the parent
  set(output_source "${OUTPUT_DIRECTORY}/${NAME}.cc")
  set(output_header "${OUTPUT_DIRECTORY}/${NAME}.h")
  set(${NAME}_source "${output_source}" PARENT_SCOPE)
  set(${NAME}_header "${output_header}" PARENT_SCOPE)

  add_custom_command(
    OUTPUT ${output_source}
      ${output_header}
    DEPENDS ${INPUT}
    COMMAND python "${FIREBASE_SCRIPT_DIR}/binary_to_array.py"
      "--input=${INPUT}"
      "--output_header=${output_header}"
      "--output_source=${output_source}"
      "--cpp_namespace=${CPP_NAMESPACE}"
      "--array=${NAME}_data"
      "--array_size=${NAME}_size"
      "--filename_identifier=${NAME}_filename"
    COMMENT "Generating ${NAME}"
  )
endfunction()