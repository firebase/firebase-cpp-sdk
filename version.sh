#!/bin/bash
#
# Copyright 2016 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Displays Pod dependency version numbers.

# Used to generate Pod dependencies in the Firebase Unity plugin.
declare -A POD_SDK_VERSIONS=(
  ["released"]="7.0.0"  # gen_build.sh: [FIR] Managed by a script. DO NOT EDIT.
  ["stable"]="7.0.0"  # gen_build.sh: [FIR] Managed by a script. DO NOT EDIT.
  ["head"]="7.0.0"  # gen_build.sh: [FIR] Managed by a script. DO NOT EDIT.
)

help() {
  echo "Usage: $(basename "$0") -c released|stable|head

Display SDK version strings for each build configuration.  In the form:

pod_sdk_version=POD_SDK_VERSION;

The output of this script can be \"eval'd\" by a bash shell so these
variables can be referenced to retrieve the appropriate SDK version.
" >&2
  exit 1
}

# See help() for the docs.
main() {
  local config=
  while getopts "c:h" option "$@"; do
    case "${option}" in
      c ) config="${OPTARG}";;
      h ) help ;;
      * ) echo "Unknown option ${option}" >&2 && help ;;
    esac
  done

  # Verify a valid version was selected.
  local pod_version="${POD_SDK_VERSIONS[${config}]}"
  if [[ -z "${pod_version}" ]]; then
    echo "Unknown config \"${config}\"" >&2 && help
  fi

  # Print versions in a eval friendly value.
  echo -n "\
pod_sdk_version=${pod_version};
"
}

main "$@"
