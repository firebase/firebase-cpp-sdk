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
import os
import pathlib
import re
from typing import List, Tuple


VERSION_PATTERN = r"\s*set\s*\(\s*version\s+([^)\s]+)\s*\)\s*"
VERSION_EXPR = re.compile(VERSION_PATTERN, re.IGNORECASE)
URL_HASH_PATTERN = r"\s*URL_HASH\s+([0-9a-zA-Z=]+)\s*"
URL_HASH_EXPR = re.compile(URL_HASH_PATTERN, re.IGNORECASE)


def main() -> None:
  (src_file, dest_file) = parse_args()

  leveldb_version = load_value(src_file, VERSION_EXPR)
  url_hash = load_value(src_file, URL_HASH_EXPR)

  set_value(dest_file, VERSION_EXPR, leveldb_version)
  set_value(dest_file, URL_HASH_EXPR, url_hash)


def parse_args() -> Tuple[pathlib.Path, pathlib.Path]:
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

  return (leveldb_cmake_src_file, leveldb_cmake_dest_file)


def load_value(file_: pathlib.Path, expr: re.Pattern) -> str:
  value = None
  value_line = None
  value_line_number = None

  with file_.open("rt", encoding="utf8") as f:
    for (line_number, line) in enumerate(f, start=1):
      match = expr.fullmatch(line)
      if not match:
        continue
      elif value is not None:
        raise RegexMatchError(
          f"Multiple lines matching regex {expr.pattern} found in "
          f"{file_}: line {value_line_number}, {value_line.strip()} "
          f"and line {line_number}, {line.strip()}")

      value = match.group(1).strip()
      value_line = line
      value_line_number = line_number

  if value is None:
    raise RegexMatchError(
      f"No line matching regex {expr.pattern} found in {file_}")

  return value


def set_value(file_: pathlib.Path, expr: re.Pattern, version: str) -> None:
  with file_.open("rt", encoding="utf8") as f:
    lines = list(f)

  matching_line = None
  matching_line_number = None

  for (line_number, line) in enumerate(lines, start=1):
    match = expr.fullmatch(line)
    if not match:
      continue
    elif matching_line is not None:
      raise RegexMatchError(
        f"Multiple lines matching regex {expr.pattern} found in "
        f"{file_}: line {matching_line_number}, {matching_line.strip()} "
        f"and line {line_number}, {line.strip()}")

    lines[line_number - 1] = line[:match.start(1)] + version + line[match.end(1):]
    matching_line = line
    matching_line_number = line_number

  if matching_line is None:
    raise RegexMatchError(
      f"No line matching regex {expr.pattern} found in {file_}")

  with file_.open("wt", encoding="utf8") as f:
    f.writelines(lines)


class RegexMatchError(Exception):
  pass


if __name__ == "__main__":
  main()
