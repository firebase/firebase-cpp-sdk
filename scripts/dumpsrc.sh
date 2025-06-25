#!/bin/bash
# Copyright 2025 Google LLC
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

# Print a formatted list of source files from specific directories.
# Suggested usage: dumpsrc.sh <path1> [path2] [path3] [...] | pbcopy

included_files=(
    '*.swig'
    '*.i'
    '*.c'
    '*.cc'
    '*.cpp'
    '*.cs'
    '*.cmake'
    '*.fbs'
    '*.gradle'
    '*.h'
    '*.hh'
    '*.java'
    '*.js'
    '*.json'
    '*.md'
    '*.m'
    '*.mm'
    'CMakeLists.txt'
    'Podfile'
)

get_markdown_language_for_file() {
  local filename="$1"
  local base="$(basename "$filename")"
  local ext="${base##*.}"
  if [[ "$base" == "$ext" && "$base" != .* ]]; then
      ext="" # No extension
  fi

  # Handle special filename case first
  if [[ "$base" == "CMakeLists.txt" ]]; then
    echo "cmake"
    return 0
  fi

  # Main logic using case based on extension
  case "$ext" in
    c)          echo "c" ;;
    cc)         echo "cpp" ;;
    cs)         echo "csharp" ;;
    hh)         echo "cpp" ;;
    m)          echo "objectivec" ;;
    mm)         echo "objectivec" ;;
    sh)         echo "bash" ;;
    py)         echo "python" ;;
    md)         echo "markdown" ;;
    json)       echo "js" ;;
    h)
        if grep -qE "\@interface|\#import" "$filename"; then
            echo "objectivec";
        else
            echo "cpp";
        fi
      ;;
    "") # Explicitly handle no extension (after CMakeLists.txt check)
        : # Output nothing
      ;;
    *) # Default case for any other non-empty extension
       echo "$ext"
       ;;
  esac
  return 0
}


if [[ -z "$1" ]]; then
    echo "Usage: $0 <path1> [path2] [path3] ..."
    exit 1
fi

find_cmd_args=('-name' 'UNUSED')

for pattern in "${included_files[@]}"; do
    if [[ -n "${pattern}" ]]; then
	find_cmd_args+=('-or' '-name' "$pattern")
    fi
done

for f in `find $* -type f -and "${find_cmd_args[@]}"`; do
    echo "*** BEGIN CONTENTS OF FILE '$f' ***";
    echo '```'$(get_markdown_language_for_file "$f");
    cat "$f";
    echo '```';
    echo "*** END CONTENTS OF FILE '$f' ***";
    echo
done
