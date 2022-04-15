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

import os
import pathlib
import re
import shutil
import tempfile
from typing import Iterable
import unittest
import unittest.mock

import firestore_patch


class TestUtilsMixin:

  def create_temp_file_with_lines(self, lines: Iterable[str]) -> pathlib.Path:
    (handle, path_str) = tempfile.mkstemp()
    os.close(handle)
    path = pathlib.Path(path_str)
    self.addCleanup(path.unlink, missing_ok=True) # pytype: disable=attribute-error

    with path.open("wt", encoding="utf8") as f:
      for line in lines:
        print(line, file=f)

    return path

  def create_temp_file_with_line(self, line: str) -> pathlib.Path:
    return self.create_temp_file_with_lines([line])


class MainTest(TestUtilsMixin, unittest.TestCase):

  def test(self):
    src_file = self.create_temp_file_with_lines([
      "aaa",
      "bbb",
      "set(version 1.2.3)",
      "URL_HASH SHA256=abcdef",
      "ccc",
      "ddd",
    ])
    dest_file = self.create_temp_file_with_lines([
      "aaa",
      "bbb",
      "set(version 4.5.6)",
      "URL_HASH SHA256=ghijkl",
      "ccc",
      "ddd",
    ])
    dest_file_contents_before = dest_file.read_text(encoding="utf8")
    patcher = unittest.mock.patch.object(
      firestore_patch,
      "parse_args",
      spec_set=True,
      autospec=True,
      return_value=(src_file, dest_file)
    )

    with patcher:
      firestore_patch.main()

    dest_file_contents_after = dest_file.read_text(encoding="utf8")
    self.assertEqual(
      dest_file_contents_after,
      dest_file_contents_before
        .replace("4.5.6", "1.2.3")
        .replace("ghijkl", "abcdef")
    )


class VersionExprTest(TestUtilsMixin, unittest.TestCase):

  def test_matches_semantic_version(self):
    path = self.create_temp_file_with_line("set(version 1.2.3)")

    value = firestore_patch.load_value(path, firestore_patch.VERSION_EXPR)

    self.assertEqual(value, "1.2.3")

  def test_matches_sha1(self):
    path = self.create_temp_file_with_line("set(version fd054fa01)")

    value = firestore_patch.load_value(path, firestore_patch.VERSION_EXPR)

    self.assertEqual(value, "fd054fa01")

  def test_ignores_whitespace(self):
    path = self.create_temp_file_with_line("  set  (  version   1.2.3  )   ")

    value = firestore_patch.load_value(path, firestore_patch.VERSION_EXPR)

    self.assertEqual(value, "1.2.3")

  def test_case_insensitive(self):
    path = self.create_temp_file_with_line("sEt(vErSiOn 1.2.3)")

    value = firestore_patch.load_value(path, firestore_patch.VERSION_EXPR)

    self.assertEqual(value, "1.2.3")


class UrlHashExprTest(TestUtilsMixin, unittest.TestCase):

  def test_matches_sha256(self):
    path = self.create_temp_file_with_line("URL_HASH SHA256=abc123def456")

    value = firestore_patch.load_value(path, firestore_patch.URL_HASH_EXPR)

    self.assertEqual(value, "SHA256=abc123def456")

  def test_ignores_whitespace(self):
    path = self.create_temp_file_with_line("  URL_HASH   abc123def456  ")

    value = firestore_patch.load_value(path, firestore_patch.URL_HASH_EXPR)

    self.assertEqual(value, "abc123def456")

  def test_case_insensitive(self):
    path = self.create_temp_file_with_line("UrL_hAsH Sha256=abc123def456")

    value = firestore_patch.load_value(path, firestore_patch.URL_HASH_EXPR)

    self.assertEqual(value, "Sha256=abc123def456")


class LoadValueTest(TestUtilsMixin, unittest.TestCase):

  def test_loads_the_value(self):
    path = self.create_temp_file_with_line("aaa1234ccc")
    expr = re.compile(r"\D+(\d+)\D+")

    value = firestore_patch.load_value(path, expr)

    self.assertEqual(value, "1234")

  def test_loads_the_value_ignoring_non_matching_lines(self):
    path = self.create_temp_file_with_lines([
      "blah",
      "blah",
      "aaa1234cccc",
      "blah",
      "blah",
    ])
    expr = re.compile(r"\D+(\d+)\D+")

    value = firestore_patch.load_value(path, expr)

    self.assertEqual(value, "1234")

  def test_raises_error_if_no_matching_line_found(self):
    path = self.create_temp_file_with_lines([
      "blah",
      "blah",
      "blah",
      "blah",
    ])
    expr = re.compile(r"\D+(\d+)\D+")

    with self.assertRaises(firestore_patch.RegexMatchError) as cm:
      firestore_patch.load_value(path, expr)

    self.assertIn("no line matching", str(cm.exception).lower())
    self.assertIn(expr.pattern, str(cm.exception))
    self.assertIn(str(path), str(cm.exception))

  def test_raises_error_if_multiple_matching_lines_found(self):
    lines = ["blah"] * 100
    lines.append("aaa123bbb")
    lines.append("ccc456ddd")
    path = self.create_temp_file_with_lines(lines)
    expr = re.compile(r"\D+(\d+)\D+")

    with self.assertRaises(firestore_patch.RegexMatchError) as cm:
      firestore_patch.load_value(path, expr)

    self.assertIn("multiple lines matching", str(cm.exception).lower())
    self.assertIn(str(path), str(cm.exception))
    self.assertIn(expr.pattern, str(cm.exception))
    self.assertIn("line 101", str(cm.exception))
    self.assertIn("line 102", str(cm.exception))
    self.assertIn("123", str(cm.exception))
    self.assertIn("456", str(cm.exception))


class SaveValueTest(TestUtilsMixin, unittest.TestCase):

  def test_saves_the_value(self):
    path = self.create_temp_file_with_line("aaa1234ccc")
    expr = re.compile(r"\D+(\d+)\D+")

    firestore_patch.set_value(path, expr, "9876")

    new_value = firestore_patch.load_value(path, expr)
    self.assertEqual(new_value, "9876")

  def test_saves_the_value_ignoring_non_matching_lines(self):
    path = self.create_temp_file_with_lines([
      "blah",
      "blah",
      "aaa1234cccc",
      "blah",
      "blah",
    ])
    expr = re.compile(r"\D+(\d+)\D+")
    file_contents_before = path.read_text(encoding="utf8")

    firestore_patch.set_value(path, expr, "9876")

    file_contents_after = path.read_text(encoding="utf8")
    self.assertEqual(
      file_contents_after,
      file_contents_before.replace("1234", "9876"),
    )

  def test_raises_error_if_no_matching_line_found(self):
    path = self.create_temp_file_with_lines([
      "blah",
      "blah",
      "blah",
      "blah",
    ])
    expr = re.compile(r"\D+(\d+)\D+")

    with self.assertRaises(firestore_patch.RegexMatchError) as cm:
      firestore_patch.set_value(path, expr, "")

    self.assertIn("no line matching", str(cm.exception).lower())
    self.assertIn(expr.pattern, str(cm.exception))
    self.assertIn(str(path), str(cm.exception))

  def test_raises_error_if_multiple_matching_lines_found(self):
    lines = ["blah"] * 100
    lines.append("aaa123bbb")
    lines.append("ccc456ddd")
    path = self.create_temp_file_with_lines(lines)
    expr = re.compile(r"\D+(\d+)\D+")

    with self.assertRaises(firestore_patch.RegexMatchError) as cm:
      firestore_patch.set_value(path, expr, "")

    self.assertIn("multiple lines matching", str(cm.exception).lower())
    self.assertIn(str(path), str(cm.exception))
    self.assertIn(expr.pattern, str(cm.exception))
    self.assertIn("line 101", str(cm.exception))
    self.assertIn("line 102", str(cm.exception))
    self.assertIn("123", str(cm.exception))
    self.assertIn("456", str(cm.exception))


if __name__ == "__main__":
  unittest.main()
