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
Ensures that there is exactly one version of cmake installed in the Android SDK,
and that that version is the correct version.

Doing this ensures that Gradle builds use a supported version of cmake.
"""

from __future__ import annotations

import dataclasses
import enum
import io
import logging
import os
import pathlib
import platform
import sys
import subprocess
import tempfile
from typing import FrozenSet, IO, Iterable, MutableSet, Optional


def main() -> None:
  if len(sys.argv) > 1:
    raise Exception(f"unexpected argument: {sys.argv[1]}")

  logging.basicConfig(
    level=logging.INFO,
    format=os.path.basename(__file__) + " %(message)s",
  )

  setup = AndroidSdkCmakeSetup()
  setup.run()


@dataclasses.dataclass(frozen=True)
class CmakeVersions:
  installed: FrozenSet[str]
  available: FrozenSet[str]


class AndroidSdkCmakeSetup:

  REQUIRED_CMAKE_VERSION = "cmake;3.18.1"

  def run(self) -> None:
    cmake_versions = self.get_cmake_versions_from_sdkmanager()
    self.log_cmake_versions(cmake_versions)
    if self.REQUIRED_CMAKE_VERSION not in cmake_versions.available:
      raise Exception(
        "The required cmake version is not available in sdkmanager: "
        + self.REQUIRED_CMAKE_VERSION
      )

    self.uninstall_unwanted_cmake_versions(cmake_versions)
    self.install_wanted_cmake_version(cmake_versions)

    cmake_versions_after = self.get_cmake_versions_from_sdkmanager()
    if cmake_versions_after.installed != frozenset([self.REQUIRED_CMAKE_VERSION]):
      raise Exception(
        "Unexpected version(s) of cmake installed in sdkmanager: "
        + ", ".join(sorted(cmake_versions_after.installed))
        + " (expected " + self.REQUIRED_CMAKE_VERSION + ")")

    logging.info(
      "sdkmanager successfully configured the one and only installed "
      "cmake version: %s",
      self.REQUIRED_CMAKE_VERSION
    )

  def get_cmake_versions_from_sdkmanager(self) -> CmakeVersions:
    with tempfile.TemporaryFile() as stdout_tempfile:
      self.run_sdkmanager(["--list"], stdout=stdout_tempfile)
      stdout_tempfile.seek(0)
      sdkmanager_output = stdout_tempfile.read().decode("utf8", errors="ignore")

    installed_cmakes: MutableSet[str] = set()
    available_cmakes: MutableSet[str] = set()
    with io.StringIO(sdkmanager_output) as f:
      current_cmakes = installed_cmakes
      for line in f:
        if line.strip() == "Available Packages:":
          current_cmakes = available_cmakes
        if line.strip().startswith("cmake;"):
          cmake_version = line.split("|")[0].strip()
          current_cmakes.add(cmake_version)

      if current_cmakes is installed_cmakes:
        raise Exception(f"Parsing sdkmanager output failed")

    return CmakeVersions(
      installed=frozenset(installed_cmakes),
      available=frozenset(available_cmakes),
    )

  def log_cmake_versions(self, cmake_versions: CmakeVersions) -> None:
    logging.info(
      "sdkmanager reported installed cmake versions (%s):",
      len(cmake_versions.installed),
    )
    for (i, version) in enumerate(sorted(cmake_versions.installed), start=1):
      logging.info("  %s. %s", i, version)

    logging.info(
      "sdkmanager reported available cmake versions (%s):",
      len(cmake_versions.available),
    )
    for (i, version) in enumerate(sorted(cmake_versions.available), start=1):
      logging.info("  %s. %s", i, version)

  def uninstall_unwanted_cmake_versions(self, cmake_versions: CmakeVersions) -> None:
    cmake_versions_to_uninstall = [
      version
      for version in cmake_versions.installed
      if version != self.REQUIRED_CMAKE_VERSION
    ]
    if not cmake_versions_to_uninstall:
      logging.info(
        "No undesired installed versions of cmake installed in sdkmanager; "
        "nothing to uninstall"
      )
      return

    cmake_versions_to_uninstall.sort()
    logging.info(
      "Uninstalling %s cmake versions: %s",
      len(cmake_versions_to_uninstall),
      ", ".join(cmake_versions_to_uninstall)
    )
    self.run_sdkmanager(["--uninstall"] + cmake_versions_to_uninstall)

  def install_wanted_cmake_version(self, cmake_versions: CmakeVersions) -> None:
    if self.REQUIRED_CMAKE_VERSION in cmake_versions.installed:
      logging.info(
        "The desired cmake version is already installed in sdkmanager: %s",
        self.REQUIRED_CMAKE_VERSION
      )
      return

    logging.info("Installing cmake version: %s", self.REQUIRED_CMAKE_VERSION)
    self.run_sdkmanager(["--install", self.REQUIRED_CMAKE_VERSION])

  def run_sdkmanager(
      self,
      args: Iterable[str],
      stdout: Optional[IO[bytes]] = None,
  ) -> None:
    self.run_command(self.sdkmanager_path(), args, stdout=stdout)

  def run_command(
      self,
      executable: pathlib.Path,
      args: Iterable[str],
      stdout: Optional[IO[bytes]] = None,
  ) -> None:
    subprocess_args = [str(executable)]
    subprocess_args.extend(args)
    subprocess_args_str = subprocess.list2cmdline(subprocess_args)
    logging.info("Running command: %s", subprocess_args_str)

    with tempfile.TemporaryFile() as stderr_file:
      subprocess_ok = False
      try:
        subprocess.check_call(subprocess_args, stdout=stdout, stderr=stderr_file)
        subprocess_ok = True
      finally:
        if not subprocess_ok:
          stderr_file.seek(0)
          print(stderr_file.read().decode("utf8", errors="ignore"), file=sys.stderr)

  def sdkmanager_path(self) -> pathlib.Path:
    android_home_str = os.environ.get("ANDROID_HOME")
    if not android_home_str:
      raise Exception("ANDROID_HOME environment variable is not set")

    android_home = pathlib.Path(android_home_str)

    if Platform.is_windows():
      sdkmanager_path = android_home / "tools" / "bin" / "sdkmanager.exe"
    else:
      sdkmanager_path = android_home / "tools" / "bin" / "sdkmanager"

    if not sdkmanager_path.exists():
      raise Exception(f"file not found: {sdkmanager_path}")

    return sdkmanager_path


class Platform(enum.Enum):
  WINDOWS = "Windows"
  LINUX = "Linux"
  MACOS = "macOS"

  @classmethod
  def current(cls) -> Platform:
    system_name = platform.system()
    if system_name == "Windows":
      return cls.WINDOWS
    elif system_name == "Linux":
      return cls.LINUX
    elif system_name == "Darwin":
      return cls.MACOS
    else:
      raise Exception(f"unknown return value from platform.system(): {system_name}")

  @classmethod
  def is_windows(cls) -> bool:
    return cls.current() == cls.WINDOWS


if __name__ == "__main__":
  main()
