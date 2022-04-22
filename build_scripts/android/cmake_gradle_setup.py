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
import platform
import re
import sys
import subprocess
import tempfile
from typing import Iterable, Tuple
import venv


def main() -> None:
  if len(sys.argv) > 1:
    raise Exception(f"unexpected argument: {sys.argv[1]}")

  logging.basicConfig(
    level=logging.INFO,
    format=os.path.basename(__file__) + " %(message)s",
  )

  run()


def run() -> None:
  (cmake_version, cmake_dir) = install_cmake()

  build_gradle_cmake_files = tuple(sorted(iter_build_gradle_cmake_files()))

  expr = re.compile(r"(\s*)path\s+'.*CMakeLists.txt'(\s*)")
  for build_gradle_file in build_gradle_cmake_files:
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

  cmake_dir_local_properties_line = "cmake.dir=" + re.sub(r"([:\\])", r"\\\1", str(cmake_dir.resolve()))
  for build_gradle_file in (pathlib.Path.cwd() / "build.gradle",) + build_gradle_cmake_files:
    local_properties_file = build_gradle_file.parent / "local.properties"
    logging.info("Setting %s in %s", cmake_dir_local_properties_line, local_properties_file)
    with local_properties_file.open("at", encoding="utf8") as f:
      print("", file=f)
      print(cmake_dir_local_properties_line, file=f)


def iter_build_gradle_cmake_files() -> Iterable[pathlib.Path]:
  for build_gradle_path in pathlib.Path.cwd().glob("**/build.gradle"):
    with build_gradle_path.open("rt", encoding="utf8", errors="ignore") as f:
      build_gradle_text = f.read()
    if "externalNativeBuild" in build_gradle_text and "cmake" in build_gradle_text:
      yield build_gradle_path


def install_cmake() -> Tuple[str, pathlib.Path]:
  cmake_version = "3.18.0"
  venv_path_str = tempfile.mkdtemp(prefix=f"cmake-{cmake_version}_", dir=pathlib.Path.cwd())
  venv_path = pathlib.Path(venv_path_str)
  logging.info("Creating a Python virtualenv for cmake %s in %s", cmake_version, venv_path)

  venv.create(venv_path, with_pip=True)

  if platform.system() == "Windows":
    pip_exe = venv_path / "Scripts" / "pip.exe"
    cmake_exe = venv_path / "Scripts" / "cmake.exe"
  else:
    pip_exe = venv_path / "bin" / "pip"
    cmake_exe = venv_path / "bin" / "cmake"

  logging.info("Installing cmake %s in %s using %s", cmake_version, venv_path, pip_exe)
  subprocess.check_output([str(pip_exe), "install", f"cmake=={cmake_version}"])

  if not cmake_exe.exists():
    raise Exception(f"File not found: {cmake_exe}")

  return (cmake_version, venv_path)


if __name__ == "__main__":
  main()
