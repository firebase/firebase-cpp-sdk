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

import firestore_patch
import pathlib
import shutil
import tempfile
import unittest


class LoadLevelDbVersionTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    temp_dir = pathlib.Path(tempfile.mkdtemp())
    self.addCleanup(shutil.rmtree, temp_dir)
    self.temp_file = temp_dir / "temp_file.txt"

  def test_loads_correct_version(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("blah1", file=f)
      print("set(version 1.23)", file=f)
      print("blah2", file=f)

    version = firestore_patch.load_leveldb_version(self.temp_file)

    self.assertEqual(version, "1.23")

  def test_ignores_whitespace(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("  set  (  version   1.23  )  ", file=f)

    version = firestore_patch.load_leveldb_version(self.temp_file)

    self.assertEqual(version, "1.23")

  def test_case_insensitive(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("SeT(vErSiOn 1.23)", file=f)

    version = firestore_patch.load_leveldb_version(self.temp_file)

    self.assertEqual(version, "1.23")

  def test_version_not_found_raises_error(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("aaa", file=f)
      print("bbb", file=f)

    with self.assertRaises(firestore_patch.LevelDbVersionLineError) as cm:
      firestore_patch.load_leveldb_version(self.temp_file)

    self.assertIn("no line matching", str(cm.exception).lower())
    self.assertIn(str(self.temp_file), str(cm.exception))

  def test_multiple_version_lines_found_raises_error(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      for i in range(100):
        print(f"line {i+1}", file=f)
      print("set(version aaa)", file=f)
      print("set(version bbb)", file=f)

    with self.assertRaises(firestore_patch.LevelDbVersionLineError) as cm:
      firestore_patch.load_leveldb_version(self.temp_file)

    self.assertIn("multiple lines matching", str(cm.exception).lower())
    self.assertIn(str(self.temp_file), str(cm.exception))
    self.assertIn("line 101", str(cm.exception))
    self.assertIn("line 102", str(cm.exception))
    self.assertIn("aaa", str(cm.exception))
    self.assertIn("bbb", str(cm.exception))


class SetLevelDbVersionTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    temp_dir = pathlib.Path(tempfile.mkdtemp())
    self.addCleanup(shutil.rmtree, temp_dir)
    self.temp_file = temp_dir / "temp_file.txt"

  def test_saves_correct_version(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("set(version asdfasdf)", file=f)

    firestore_patch.set_leveldb_version(self.temp_file, "1.2.3")

    new_version = firestore_patch.load_leveldb_version(self.temp_file)
    self.assertEqual(new_version, "1.2.3")

  def test_case_insensitivity(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("sEt(vErSiOn asdfasdf)", file=f)

    firestore_patch.set_leveldb_version(self.temp_file, "1.2.3")

    new_version = firestore_patch.load_leveldb_version(self.temp_file)
    self.assertEqual(new_version, "1.2.3")

  def test_leaves_whitespace_alone(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("  set  (   version 1.2.3.4    )   ", file=f)
    temp_file_contents = self.temp_file.read_text(encoding="utf8")

    firestore_patch.set_leveldb_version(self.temp_file, "a.b.c.d")

    temp_file_contents_after = self.temp_file.read_text(encoding="utf8")
    expected_temp_file_contents = temp_file_contents.replace("1.2.3.4", "a.b.c.d")
    self.assertEqual(temp_file_contents_after, expected_temp_file_contents)

  def test_does_not_touch_other_lines(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("blah1", file=f)
      print("set(version 1.2.3.4)", file=f)
      print("blah2", file=f)
    temp_file_contents = self.temp_file.read_text(encoding="utf8")

    firestore_patch.set_leveldb_version(self.temp_file, "a.b.c.d")

    temp_file_contents_after = self.temp_file.read_text(encoding="utf8")
    expected_temp_file_contents = temp_file_contents.replace("1.2.3.4", "a.b.c.d")
    self.assertEqual(temp_file_contents_after, expected_temp_file_contents)

  def test_version_not_found_raises_error(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      print("aaa", file=f)
      print("bbb", file=f)

    with self.assertRaises(firestore_patch.LevelDbVersionLineError) as cm:
      firestore_patch.set_leveldb_version(self.temp_file, "a.b.c")

    self.assertIn("no line matching", str(cm.exception).lower())
    self.assertIn(str(self.temp_file), str(cm.exception))

  def test_multiple_version_lines_found_raises_error(self):
    with self.temp_file.open("wt", encoding="utf8") as f:
      for i in range(100):
        print(f"line {i+1}", file=f)
      print("set(version aaa)", file=f)
      print("set(version bbb)", file=f)

    with self.assertRaises(firestore_patch.LevelDbVersionLineError) as cm:
      firestore_patch.set_leveldb_version(self.temp_file, "a.b.c")

    self.assertIn("multiple lines matching", str(cm.exception).lower())
    self.assertIn(str(self.temp_file), str(cm.exception))
    self.assertIn("101, 102", str(cm.exception))


if __name__ == "__main__":
  unittest.main()
