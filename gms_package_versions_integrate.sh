#!/bin/bash -eu
#
# Copyright 2019 Google LLC
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

help_and_exit() {
  echo "\
Integrates Android libraries referenced by the Firebase C++ SDK into a release
branch.

$(basename "$0") \\
  -c citc_client_to_query_for_android_libraries \\
  [-t target_citc_client_to_add_android_libraries_to] \\
  [-b branch_name] \\
  [-d] \\
  [-h]

-c citc_client_to_query_for_android_libraries:
  Name of the CITC client to query for Android libraries.  This will perform
  a blaze query at the head of this client.
-t target_citc_client_to_add_android_libraries_to:
  Name of the CITC client to add Android libraries to.
-b branch_name:
  Name of the branch in the target CITC client to modify.
  e.g firebase.cpp_release_branch/281913092.1
  You can retrieve the branch associated with a candidate by:
  - Navigating to a Rapid project in the web UI http://rapid
  - Reading the Branch string
  You can checkout a candidate's branch using 'rapid-tool checkout_branch'
-d:
  Print branch commands without executing them.
-h:
  Display this help message.
"
  exit 1
}

main() {
  local citc_client=
  local target_citc_client=
  local branch=""
  local dryrun=0
  while getopts "c:t:b:dh" option "$@"; do
    case ${option} in
      c ) citc_client="${OPTARG}";;
      t ) target_citc_client="${OPTARG}";;
      b ) branch="${OPTARG}";;
      d ) dryrun=1;;
      h ) help_and_exit;;
      * ) help_and_exit;;
    esac
  done

  # Retrieve the set of Android library directories.
  local -a android_library_dirs=($(
    cd "$(p4 g4d "${citc_client}")" >/dev/null &&
    blaze query '
    filter("//third_party/java/android_libs/gcore/prebuilts/.*",
      kind(android_library,
        deps(filter("firebase/(?!invites).*/client/cpp:.*android.*deps",
          //firebase/...))))' | \
            sed 's@^//@@;s@:android_library$@@'))

  # If a target branch and CITC client is specified modify the branch spec.
  if [[ "${branch}" != "" && "${target_citc_client}" != "" ]]; then
    (
      cd "$(p4 g4d "${target_citc_client}")/../branches/${branch}"
      for android_library_dir in "${android_library_dirs[@]}"; do
        echo "${android_library_dir}" >&2
        for mapping in \
          //depot/google3/"${android_library_dir}"'/...' \
          //depot/google3/"$(echo "${android_library_dir}" | \
                             sed -r 's@/x[0-9]+_.*@@')"'/*'; do
          if [[ $((dryrun)) -eq 1 ]]; then
            echo g4 branch "${branch}" -a "${mapping}"
          else
            g4 branch "${branch}" -a "${mapping}"
            # Integrate all files added to the branchspec's view.
            g4 integrate "${mapping}" \
              "$(echo "${mapping}" | \
                 sed "s@/google3/@/branches/${branch}/google3/@")"
          fi
        done
      done
      echo "Branch spec modified in $(pwd)"
    )
  else
    # Just print the Android library package directories.
    for android_library_dir in "${android_library_dirs[@]}"; do
      echo "${android_library_dir}"
    done
  fi
}

main "$@"
