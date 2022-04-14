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
Modify the version in leveldb.cmake from the Firebase iOS SDK to match the
version from this C++ SDK.
"""

import argparse
import dataclasses
import os
import pathlib
import re
from typing import Iterable, Sequence


VERSION_PATTERN = r"\s*set\s*\(\s*version\s+([^)\s]+)\s*\)\s*"
VERSION_EXPR = re.compile(VERSION_PATTERN, re.IGNORECASE)


def main() -> None:
  args = parse_args()
  leveldb_version = load_leveldb_version(args.leveldb_cmake_src_file)
  set_leveldb_version(args.leveldb_cmake_dest_file, leveldb_version)


@dataclasses.dataclass(frozen=True)
class ParsedArgs:
  leveldb_cmake_src_file: pathlib.Path
  leveldb_cmake_dest_file: pathlib.Path


def parse_args() -> ParsedArgs:
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument("--leveldb-version-from", required=True)
  arg_parser.add_argument("--leveldb-version-to")

  parsed_args = arg_parser.parse_args()

  leveldb_cmake_src_file = pathlib.Path(parsed_args.leveldb_version_from)

  if parsed_args.leveldb_version_to:
    leveldb_cmake_dest_file = pathlib.Path(parsed_args.leveldb_version_to)
  else:
    leveldb_cmake_dest_file = pathlib.Path.cwd() \
      / "cmake" / "external" / "leveldb.cmake"

  return ParsedArgs(
    leveldb_cmake_src_file=leveldb_cmake_src_file,
    leveldb_cmake_dest_file=leveldb_cmake_dest_file,
  )


def load_leveldb_version(cmake_file: pathlib.Path) -> str:
  version = None
  version_line = None
  version_line_number = None

  with cmake_file.open("rt", encoding="utf8") as f:
    for (line_number, line) in enumerate(f, start=1):
      match = VERSION_EXPR.fullmatch(line)
      if match:
        if version is not None:
          raise LevelDbVersionLineError(
            f"Multiple lines matching regex {VERSION_EXPR.pattern} found in "
            f"{cmake_file}: line {version_line_number}, {version_line.strip()} "
            f"and line {line_number}, {line.strip()}")
        version = match.group(1).strip()
        version_line = line
        version_line_number = line_number

  if version is None:
    raise LevelDbVersionLineError(
      f"No line matching regex {VERSION_EXPR.pattern} found in {cmake_file}")

  return version


def set_leveldb_version(cmake_file: pathlib.Path, version: str) -> str:
  with cmake_file.open("rt", encoding="utf8") as f:
    lines = list(f)

  version_lines_found = []
  for (i, line) in enumerate(lines):
    match = VERSION_EXPR.fullmatch(line)
    if match:
      lines[i] = line[:match.start(1)] + version + line[match.end(1):]
      version_lines_found.append(i)

  if len(version_lines_found) == 0:
    raise LevelDbVersionLineError(
      f"No line matching regex {VERSION_EXPR.pattern} found in {cmake_file}")
  elif len(version_lines_found) > 1:
    raise LevelDbVersionLineError(
      f"Multiple lines matching regex {VERSION_EXPR.pattern} found in "
      f"{cmake_file}: {', '.join(str(i+1) for i in version_lines_found)}")

  with cmake_file.open("wt", encoding="utf8") as f:
    f.writelines(lines)


class LevelDbVersionLineError(Exception):
  pass



if __name__ == "__main__":
  main()
