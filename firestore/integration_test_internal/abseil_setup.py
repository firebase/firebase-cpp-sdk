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
Downloads abseil-cpp source from GitHub and extracts them.

Syntax: %s <version> <dest_dir> <expected_archive_sha256>

<version> is the version of abeseil to download (e.g. 20211102.0).

<dest_dir> is the directory into which to put the abseil sources. A "src"
subdirectory will contain the root of the downloaded sources.

<expected_archive_sha256> is the expected SHA256 hash of the downloaded
abseil-cpp source archive.
"""

import hashlib
import json
import pathlib
import shutil
import tarfile
import tempfile
from typing import Sequence
import urllib.request

from absl import app
from absl import logging


def main(argv: Sequence[str]) -> None:
  if len(argv) == 1:
    raise app.UsageError("<version> must be specified")
  elif len(argv) == 2:
    raise app.UsageError("<dest_dir> must be specified")
  elif len(argv) == 3:
    raise app.UsageError("<expected_archive_sha256> must be specified")
  elif len(argv) > 4:
    raise app.UsageError(f"unexpected argument: {argv[3]}")

  downloader = AbslDownloader(
    absl_version=argv[1],
    dest_dir=pathlib.Path(argv[2]),
    expected_archive_sha256=argv[3],
  )

  downloader.run()


class AbslDownloader:

  def __init__(
    self,
    absl_version: str,
    dest_dir: pathlib.Path,
    expected_archive_sha256: str,
  ) -> None:
    self.absl_version = absl_version
    self.dest_dir = dest_dir
    self.expected_archive_sha256 = expected_archive_sha256

  def run(self) -> None:
    archive_file = self.download_archive()
    self.extract_archive(archive_file)

  def download_archive(self) -> pathlib.Path:
    archive_file = self.dest_dir / f"abseil-cpp-{self.absl_version}.tar.gz"

    if archive_file.exists():
      logging.debug(
        "Calculating sha256 hash of previously-downloaded archive %s",
        archive_file,
      )
      actual_archive_sha256 = self.calculate_sha256(archive_file)
      if actual_archive_sha256 == self.expected_archive_sha256:
        logging.debug(
          "The sha256 hash of the previously-downloaded archive %s is "
          "correct, skipping re-download of it.",
          archive_file,
        )
        return archive_file

      logging.info(
        "The sha256 hash of the previously-downloaded archive %s is %s, but "
        "expected %s; downloading it again.",
        archive_file,
        actual_archive_sha256,
        self.expected_archive_sha256,
      )

    archive_dir = archive_file.parent
    if not archive_dir.exists():
      logging.info("Creating directory: %s", archive_dir)
      archive_dir.mkdir(parents=True, exist_ok=True)

    url = "https://github.com/abseil/abseil-cpp/archive/" \
      f"{self.absl_version}.tar.gz"
    logging.info("Downloading %s to %s", url, archive_file)
    urllib.request.urlretrieve(url, archive_file)

    archive_num_bytes = archive_file.stat().st_size
    logging.info(f"Downloaded {archive_num_bytes:,} bytes")

    logging.info(
      "Calculating sha256 hash of downloaded archive %s",
      archive_file,
    )
    actual_archive_sha256 = self.calculate_sha256(archive_file)
    if actual_archive_sha256 != self.expected_archive_sha256:
      raise Sha256Mismatch(
        f"The sha256 hash of the downloaded archive {archive_file} is "
        f"{actual_archive_sha256}, but expected "
        f"{self.expected_archive_sha256}"
      )

    return archive_file

  def extract_archive(self, archive_file: pathlib.Path) -> None:
    stamp_file = self.dest_dir / "extract_stamp.txt"
    extract_dir = self.dest_dir / "src"

    if stamp_file.is_file() and extract_dir.is_dir():
      logging.debug(
        "Checking contents of %s to see if we can skip extracting the "
        "contents of %s",
        stamp_file,
        archive_file,
      )
      try:
        with stamp_file.open("rt", encoding="utf8", errors="replace") as f:
          stamp = json.load(f)
      except json.JSONDecodeError as e:
        logging.info(
          "Checking contents of %s failed to be parsed as valid json; "
          "re-extracting %s (%s)",
          stamp_file,
          archive_file,
          e,
        )
      else:
        if not isinstance(stamp, dict) or \
            set(stamp.keys()) != set(["sha256", "extract_dir"]):
          logging.info(
            "Checking contents of %s failed: invalid json contents; "
            "re-extracting %s",
            stamp_file,
            archive_file,
          )
        else:
          stamp_sha256 = stamp["sha256"]
          stamp_extract_dir = stamp["extract_dir"]
          if stamp_sha256 != self.expected_archive_sha256:
            logging.info(
              "Checking contents of %s failed: incorrect sha256; "
              "re-extracting %s",
              stamp_file,
              archive_file,
            )
          elif pathlib.Path(stamp_extract_dir).resolve() != extract_dir.resolve():
            logging.info(
              "Checking contents of %s failed: incorrect extract_dir; "
              "re-extracting %s",
              stamp_file,
              archive_file,
            )
          else:
            logging.debug(
              "Using previous extraction of %s to %s; skipping re-extraction",
              archive_file,
              extract_dir,
            )
            return

    if extract_dir.exists():
      logging.info("Deleting directory: %s", extract_dir)
      shutil.rmtree(extract_dir)

    logging.info("Creating directory: %s", extract_dir)
    extract_dir.mkdir(parents=True, exist_ok=False)

    temp_dir = pathlib.Path(tempfile.mkdtemp(dir=self.dest_dir))

    logging.info("Extracting %s to %s", archive_file, temp_dir)
    with tarfile.open(archive_file, "r") as f:
      f.extractall(temp_dir)

    logging.info("Moving files from %s to %s", temp_dir, extract_dir)
    temp_dir_entries = tuple(temp_dir.iterdir())
    if len(temp_dir_entries) != 1:
      raise Exception(f"unexpected entries extracted from {archive_file}: "
        + ", ".join(sorted(str(p) for p in temp_dir_entries)))
    temp_dir_root = temp_dir_entries[0]
    for temp_file_or_dir in temp_dir_root.iterdir():
      dest_file_or_dir = extract_dir / temp_file_or_dir.relative_to(temp_dir_root)
      logging.debug("Moving %s to %s", temp_file_or_dir, dest_file_or_dir)
      temp_file_or_dir.rename(dest_file_or_dir)

    logging.info("Creating %s", stamp_file)
    stamp = {
      "sha256": self.expected_archive_sha256,
      "extract_dir": str(extract_dir.resolve(strict=True)),
    }
    with stamp_file.open("wt", encoding="utf8") as f:
      json.dump(stamp, f)

  def calculate_sha256(self, file_path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with file_path.open("rb") as f:
      while True:
        chunk = f.read(8192)
        if not chunk:
          break
        h.update(chunk)
    return h.hexdigest()


class Sha256Mismatch(Exception):
  pass


if __name__ == "__main__":
  app.run(main)
