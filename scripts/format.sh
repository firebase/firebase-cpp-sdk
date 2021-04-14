#!/bin/bash

# Copyright 2021 Google LLC
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

# Usage:
# ./scripts/format.sh [branch-name | filenames]
#
# With no arguments, formats all eligible files in the repo
# Pass a branch name to format all eligible files changed since that branch
# Pass a specific file or directory name to format just files found there
#
# Commonly
# ./scripts/style.sh master

clang_options=(-style=file)


if [[ $# -gt 0 && "$1" == "test-only" ]]; then
  test_only=true
else
  test_only=false
  clang_options+=(-i)
fi

echo num_arguments $#

if [[ $# -gt 0 ]]; then
    if git rev-parse "$1" -- >& /dev/null; then
      # Argument was a branch name show files changed since that branch
      echo git diff --name-only --relative --diff-filter=ACMR "$1"
      git diff --name-only --relative --diff-filter=ACMR "$1"
    else
      echo defaulting to files
      # Otherwise assume the passed things are files or directories
      find "$@" -type f
    fi
  else
    # Do everything by default
    echo Do evertying by default
    find . -type f
  fi

files=$(
(
  if [[ $# -gt 0 ]]; then
    if git rev-parse "$1" -- >& /dev/null; then
      # Argument was a branch name show files changed since that branch
      echo git diff --name-only --relative --diff-filter=ACMR "$1"
      git diff --name-only --relative --diff-filter=ACMR "$1"
    else
      echo defaulting to files
      # Otherwise assume the passed things are files or directories
      find "$@" -type f
    fi
  else
    # Do everything by default
    echo Do evertying by default
    find . -type f
  fi
))


needs_formatting=false
for f in $files; do
  clang-format -output-replacements-xml "$f" | grep "<replacement " > /dev/null
  
  if [[ "$test_only" == true && $? -ne 1 ]]; then
    echo "$f needs formatting."
    needs_formatting=true
  fi
done

if [[ "$needs_formatting" == true ]]; then
  echo "Proposed commit is not style compliant."
  echo "Run scripts/style.sh and git add the result."
  exit 1
fi
