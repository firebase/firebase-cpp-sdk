#!/bin/bash
# Runs the presubmit tests that must be run locally, because they don't work in
# TAP or Guitar at the moment (see b/135205911). Execute this script before
# submitting.

set -euo pipefail

# Executes blaze to run a Kokoro test.
#
# The first argument is the name of an associative array variable. A key/value
# pair will be inserted into this associative array with information about the
# launched blaze process. The key will be the PID of the blaze process and the
# value will be the full blaze command that was executed.
#
# The second argument is the "name" of the test, and is used in the subdirectory
# name of the directory specified to blaze's --output_base argument.
#
# The remaining arguments, if any, will be specified to the blaze command after
# the "test" argument.
function run_blaze {
  if [[ $# -lt 2 ]] ; then
    echo "INTERNAL ERROR: run_blaze invalid arguments: $*" >&2
    exit 1
  fi

  local -n readonly blaze_process_map="$1"
  local readonly blaze_build_name="$2"
  shift 2

  local readonly blaze_args=(
    "blaze"
    "--blazerc=/dev/null"
    "--output_base=/tmp/run_local_tests_blaze_output_bases/${blaze_build_name}"
    "test"
    "$@"
    "//firebase/firestore/client/cpp:kokoro_build_test"
  )

  echo "${blaze_args[*]}"
  "${blaze_args[@]}" &
  blaze_process_map["$!"]="${blaze_args[*]}"
}

# Waits for blaze commands started by `run_blaze` to complete.
#
# If all blaze commands complete successfully then a "success" message is
# printed; otherwise, if one or more of the blaze commands fail, then an error
# message is printed.
#
# The first (and only) argument is the name of an associative array variable.
# This variable should be the same that was was specified to `run_blaze` and
# specifies the blaze commands for which to wait.
#
# The return value is 0 if all blaze processes completed successfully or 1 if
# one or more of the blaze processes failed.
function wait_for_blazes_to_complete {
  if [[ $# -ne 1 ]] ; then
    echo "INTERNAL ERROR: wait_for_blazes_to_complete invalid arguments: $*" >&2
    exit 1
  fi

  local -n blaze_process_map="$1"

  local blaze_pid
  local -a blaze_failed_pids=()

  for blaze_pid in "${!blaze_process_map[@]}" ; do
    if ! wait "${blaze_pid}" ; then
      blaze_failed_pids+=("${blaze_pid}")
    fi
  done

  local readonly num_failed_blazes="${#blaze_failed_pids[@]}"
  if [[ ${num_failed_blazes} -eq 0 ]] ; then
    echo "All blaze commands completed successfully."
  else
    echo "ERROR: The following ${num_failed_blazes} blaze invocation(s) failed:"
    for blaze_pid in "${blaze_failed_pids[@]}" ; do
      echo "${blaze_process_map[${blaze_pid}]}"
    done
    echo "Go to http://sponge2 to see the results."
    return 1
  fi
}

# The main program begins here.
if [[ $# -gt 0 ]] ; then
  echo "ERROR: $0 does not accept any arguments, but got: $*" >&2
  exit 2
fi

declare -A BLAZE_PROCESS_MAP

# LINT.IfChange
run_blaze "BLAZE_PROCESS_MAP" "android_arm" "--config=android_arm"
run_blaze "BLAZE_PROCESS_MAP" "darwin_x86_64" "--config=darwin_x86_64"
run_blaze "BLAZE_PROCESS_MAP" "linux" "--define=force_regular_grpc=1"
run_blaze "BLAZE_PROCESS_MAP" "msvc" "--config=msvc"
# LINT.ThenChange(//depot_firebase_cpp/firestore/client/cpp/METADATA)

wait_for_blazes_to_complete "BLAZE_PROCESS_MAP"
