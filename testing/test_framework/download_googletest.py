#!/usr/bin/python

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
"""Download GoogleTest from GitHub into a subdirectory."""

# pylint: disable=superfluous-parens,g-import-not-at-top

import io
import os
import shutil
import tempfile
import zipfile

# Import urllib in a way that is compatible with Python 2 and Python 3.
try:
  import urllib.request
  compatible_urlopen = urllib.request.urlopen
except ImportError:
  import urllib2
  compatible_urlopen = urllib2.urlopen

# Run from inside the script directory
SRC_DIR = os.path.relpath(os.path.dirname(__file__))
TAG = os.path.basename(__file__)

GOOGLETEST_ZIP = 'https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip'
# Top-level directory inside the zip file to ignore.
GOOGLETEST_DIR = os.path.join('googletest-release-1.11.0')

# The GoogleTest code is copied into this subdirectory.
# This structure matches where the files are placed by CMake.
DESTINATION_DIR = os.path.join(SRC_DIR, 'external/googletest/src')

CHECK_EXISTS = os.path.join(
    SRC_DIR, 'external/googletest/src/googletest/src/gtest-all.cc')

# Don't download it again if it already exists.
if os.path.exists(CHECK_EXISTS):
  print('%s: GoogleTest already downloaded, skipping.' % (TAG))
  exit(0)

# Download the zipfile into memory, extract into /tmp, then move into the
# current directory.

try:
  # Download to a temporary directory.
  zip_extract_path = tempfile.mkdtemp(suffix='googletestdownload')
  print('%s: Downloading GoogleTest from %s' % (TAG, GOOGLETEST_ZIP))
  zip_download = compatible_urlopen(GOOGLETEST_ZIP)
  zip_file = io.BytesIO(zip_download.read())
  print('%s: Extracting GoogleTest...' % (TAG))
  zip_ref = zipfile.ZipFile(zip_file, mode='r')
  zip_ref.extractall(zip_extract_path)
  if os.path.exists(DESTINATION_DIR):
    shutil.rmtree(DESTINATION_DIR)
  shutil.move(os.path.join(zip_extract_path, GOOGLETEST_DIR), DESTINATION_DIR)
  print('%s: Finished.' % (TAG))
except Exception as e:
  raise
finally:
  # Clean up the temp directory if we created one.
  if os.path.exists(zip_extract_path):
    shutil.rmtree(zip_extract_path)

if not os.path.exists(CHECK_EXISTS):
  print('%s: Failed to download GoogleTest to %s' % (TAG, DESTINATION_DIR))
  exit(1)
