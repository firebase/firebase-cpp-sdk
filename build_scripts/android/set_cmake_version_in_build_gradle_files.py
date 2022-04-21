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

"""
Updates all build.gradle files to explicitly specify the version of cmake to use.

This avoids gradle builds using the default cmake version 3.6 that is bundled with the
Android SDK, since our minimum-supported cmake version is 3.7.
"""

from __future__ import annotations

import io
import logging
import os
import pathlib
import re
import sys
import subprocess
import tempfile
from typing import Iterable


def main() -> None:
  if len(sys.argv) > 1:
    raise Exception(f"unexpected argument: {sys.argv[1]}")

  logging.basicConfig(
    level=logging.INFO,
    format=os.path.basename(__file__) + " %(message)s",
  )

  run()


def run() -> None:
  cmake_version = get_cmake_version()

  logging.info("Setting cmake version to %s in build.gradle files", cmake_version)
  expr = re.compile(r"(\s*)path\s+'.*CMakeLists.txt'(\s*)")

  for build_gradle_file in iter_build_gradle_cmake_files():
    logging.info("Setting cmake version to %s in %s", cmake_version, build_gradle_file)
    with build_gradle_file.open("rt", encoding="utf8") as f:
      lines = list(f)

    for i in range(len(lines)):
      line = lines[i]
      match = expr.fullmatch(line)
      if match:
        indent = match.group(1)
        eol = match.group(2)
        lines.insert(i+1, indent + "version '" + cmake_version + "'" + eol)
        break
    else:
      raise Exception(f"Unable to find place to insert cmake version in {build_gradle_file}")

    with build_gradle_file.open("wt", encoding="utf8") as f:
      f.writelines(lines)

def iter_build_gradle_cmake_files() -> Iterable[pathlib.Path]:
  for build_gradle_path in pathlib.Path.cwd().glob("**/build.gradle"):
    with build_gradle_path.open("rt", encoding="utf8", errors="ignore") as f:
      build_gradle_text = f.read()
    if "externalNativeBuild" in build_gradle_text and "cmake" in build_gradle_text:
      yield build_gradle_path

def get_cmake_version() -> str:
  args = ["cmake", "--version"]
  logging.info("Running command: %s", subprocess.list2cmdline(args))

  with tempfile.TemporaryFile() as stdout_file:
    subprocess.check_call(args, stdout=stdout_file)
    stdout_file.seek(0)
    cmake_output = stdout_file.read().decode("utf8", errors="replace")

  version_line_prefix = "cmake version"
  for line in io.StringIO(cmake_output):
    if line.startswith(version_line_prefix):
      return line[len(version_line_prefix):].strip()

  print(cmake_output)
  raise Exception(f"Unable to determine version of cmake")


if __name__ == "__main__":
  main()
