#!/usr/bin/env python3
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

"""Tests for check_duplicate_symbols.py."""

import os
import shutil
import subprocess
import tempfile
import unittest

import check_duplicate_symbols as check


class ParseNmOutputTest(unittest.TestCase):

  def test_llvm_nm_archive_format(self):
    text = "\n".join([
        "libnanopb.a[pb_encode.c.o]: _pb_encode T 100 20",
        "libnanopb.a[pb_encode.c.o]: _memcpy U",
        "libFirebaseCore.a[FIRApp.m.o]: _OBJC_CLASS_$_FIRApp S 0 28",
        "",
        "libFirebaseCore.a:",
    ])
    self.assertEqual(check.parse_nm_output(text), [
        ("libnanopb.a[pb_encode.c.o]", "_pb_encode", "T"),
        ("libnanopb.a[pb_encode.c.o]", "_memcpy", "U"),
        ("libFirebaseCore.a[FIRApp.m.o]", "_OBJC_CLASS_$_FIRApp", "S"),
    ])

  def test_universal_suffix_is_stripped(self):
    text = ("libx.a[a.o] (for architecture arm64): _foo T 0 4")
    self.assertEqual(check.parse_nm_output(text),
                     [("libx.a[a.o]", "_foo", "T")])


class FindConflictsTest(unittest.TestCase):

  def test_force_loaded_vs_linked_conflict(self):
    forced = {"libnanopb_spm.a": [("libnanopb_spm.a[pb_encode.o]",
                                   "_pb_encode", "T")]}
    linked = {"libprotobuf-nanopb.a": [("libprotobuf-nanopb.a[pb_encode.o]",
                                        "_pb_encode", "T")]}
    conflicts = check.find_conflicts(forced, linked, check.DEFAULT_IGNORES)
    self.assertEqual(list(conflicts), ["_pb_encode"])
    self.assertEqual(len(conflicts["_pb_encode"]), 2)

  def test_linked_only_duplicates_are_ignored(self):
    linked = {
        "a.a": [("a.a[x.o]", "_crc32", "T")],
        "b.a": [("b.a[y.o]", "_crc32", "T")],
    }
    self.assertEqual(check.find_conflicts({}, linked, []), {})

  def test_undefined_weak_and_ignored_symbols(self):
    forced = {"a.a": [("a.a[x.o]", "_foo", "U"),
                      ("a.a[x.o]", "__ZN3fooC1Ev", "T"),
                      ("a.a[x.o]", "_bar", "W")]}
    linked = {"b.a": [("b.a[y.o]", "_foo", "T"),
                      ("b.a[y.o]", "__ZN3fooC1Ev", "T"),
                      ("b.a[y.o]", "_bar", "T")]}
    self.assertEqual(
        check.find_conflicts(forced, linked, check.DEFAULT_IGNORES), {})

  def test_duplicates_within_one_input_are_not_reported(self):
    forced = {"a.a": [("a.a[x.o]", "_foo", "T"), ("a.a[y.o]", "_foo", "T")]}
    self.assertEqual(check.find_conflicts(forced, {}, []), {})

  def test_custom_ignore(self):
    forced = {"a.a": [("a.a[x.o]", "_pb_encode", "T")]}
    linked = {"b.a": [("b.a[y.o]", "_pb_encode", "T")]}
    self.assertEqual(check.find_conflicts(forced, linked, [r"^_pb_"]), {})


@unittest.skipUnless(shutil.which("cc") and shutil.which("ar") and
                     shutil.which("nm"), "needs a C toolchain")
class EndToEndTest(unittest.TestCase):
  """Builds real archives with the host toolchain and runs the checker."""

  def _archive(self, directory, name, source):
    c_file = os.path.join(directory, name + ".c")
    o_file = os.path.join(directory, name + ".o")
    a_file = os.path.join(directory, "lib%s.a" % name)
    with open(c_file, "w") as f:
      f.write(source)
    subprocess.run(["cc", "-c", c_file, "-o", o_file], check=True)
    subprocess.run(["ar", "rcs", a_file, o_file], check=True)
    return a_file

  def test_detects_conflict(self):
    with tempfile.TemporaryDirectory() as tmp:
      forced = self._archive(tmp, "sdk", "int pb_encode(void){return 1;}\n"
                             "int sdk_only(void){return 2;}\n")
      clean = self._archive(tmp, "clean", "int other(void){return 3;}\n")
      clashing = self._archive(tmp, "firestore_nanopb",
                               "int pb_encode(void){return 4;}\n")
      self.assertEqual(
          check.main(["--force-load", forced, "--link", clean]), 0)
      self.assertEqual(
          check.main(["--force-load", forced, "--link", clean, clashing]), 1)
      self.assertEqual(
          check.main(["--force-load", forced, "--link", clashing,
                      "--warn-only"]), 0)


if __name__ == "__main__":
  unittest.main()
