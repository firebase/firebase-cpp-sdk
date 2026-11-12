#!/bin/bash

# Copyright 2026 Google LLC
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

# Zip up a directory recursively, preserving symlinks (e.g. inside
# frameworks), as fast as possible.
#
# Info-ZIP's "zip" is single-threaded, which makes zipping the multi-gigabyte
# SDK package very slow. If 7-Zip is available, use it to create the zip file
# with multithreaded compression instead. After creating the archive, verify
# that it contains the same number of regular files and symlinks as the
# source directory; if it doesn't (for example, if this version of 7-Zip
# doesn't support storing symlinks in zip files), fall back to Info-ZIP.
#
# Usage: zip_package.sh <output.zip> <directory>
#   The directory is added to the archive using the given (relative) path,
#   so run this from the parent of <directory>.
#
# Environment variables:
#   ZIP_PACKAGE_LEVEL: Compression level, 0-9 (default: 6).
#   ZIP_PACKAGE_FORCE_INFOZIP: If set to 1, always use Info-ZIP.

set -e

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <output.zip> <directory>" 1>&2
  exit 2
fi

readonly output_zip="$1"
readonly input_dir="$2"
readonly level="${ZIP_PACKAGE_LEVEL:-6}"

if [[ ! -d "${input_dir}" ]]; then
  echo "Directory not found: ${input_dir}" 1>&2
  exit 2
fi

# Count the regular files and symlinks in the source directory.
expected_files=$(find "${input_dir}" -type f | wc -l)
expected_links=$(find "${input_dir}" -type l | wc -l)

verify_zip() {
  # zipinfo prints one line per entry, starting with the Unix mode string.
  local listing actual_files actual_links
  listing=$(zipinfo "$1" 2>/dev/null) || return 1
  actual_files=$(echo "${listing}" | grep -c '^-' || true)
  actual_links=$(echo "${listing}" | grep -c '^l' || true)
  echo "Zip contains ${actual_files} files and ${actual_links} symlinks" \
       "(expected ${expected_files} and ${expected_links})."
  [[ "${actual_files}" -eq "${expected_files}" &&
     "${actual_links}" -eq "${expected_links}" ]]
}

sevenzip=
for cmd in 7zz 7z 7za; do
  if command -v "${cmd}" > /dev/null; then
    sevenzip="${cmd}"
    break
  fi
done

rm -f "${output_zip}"
if [[ -n "${sevenzip}" && "${ZIP_PACKAGE_FORCE_INFOZIP}" != "1" ]]; then
  echo "Creating ${output_zip} with ${sevenzip} (multithreaded, level ${level})..."
  # -snl: store symlinks as links. -mmt=on: multithreaded compression.
  # -bso0 -bsp0: suppress the per-file output and progress indicator.
  if "${sevenzip}" a -tzip -mx="${level}" -mmt=on -snl -bso0 -bsp0 \
       "${output_zip}" "${input_dir}" && verify_zip "${output_zip}"; then
    exit 0
  fi
  echo "Warning: ${sevenzip} failed or produced an unexpected archive," \
       "falling back to Info-ZIP." 1>&2
  rm -f "${output_zip}"
fi

echo "Creating ${output_zip} with Info-ZIP (level ${level})..."
zip -q -"${level}" -r -y "${output_zip}" "${input_dir}"
verify_zip "${output_zip}"
