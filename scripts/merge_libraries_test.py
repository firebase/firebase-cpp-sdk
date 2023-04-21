# Copyright 2018 Google LLC
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

import os
import shutil
import subprocess
import sys
import tempfile
import merge_libraries
import warnings
from absl import flags
from absl import logging
from absl.testing import absltest

_TEST_FILE = os.path.abspath('merge_libraries_test_file.a')

FLAGS = flags.FLAGS

class MergeLibrariesTest(absltest.TestCase):
  """Verifies a subset of functionality of Blastdoor merge_libraries script."""
  def setUp(self):
    if not os.path.exists(_TEST_FILE):
      logging.info("Compiling merge_libraries_test_file.cc")
      subprocess.run(['g++', '-O2', '-std=c++11', '-c', 'scripts/merge_libraries_test_file.cc', '-o', 'test_file.o'])
      subprocess.run([FLAGS.binutils_ar_cmd, 'rcs', _TEST_FILE, 'test_file.o'])
      warnings.filterwarnings("ignore", category=ResourceWarning)

  def test_get_top_level_namespaces(self):
    """Verify that get_top_level_namespaces works."""
    test_symbols = {
        "namespace::function()": {"namespace"},
        "outer::inner::function()": {"outer"},
        "function(fparam::type)": {"fparam"},
        "templatefn<tparam::type>()": {"tparam"},
        "outer_ns::inner::templatefn<t2Param::type>(Fparam::type)": {
            "outer_ns", "t2Param", "Fparam"
        },
        "nswithclass::ClassName::ClassName()": {"nswithclass"},
        "onlydestructor::DestructorClass::~DestructorClass()": {"onlydestructor"},
        "topleveldestructor::~topleveldestructor()": {"topleveldestructor"},
        "rtti::RttiClass::`RTTI Class Hierarchy Descriptor'": {"rtti"},
        "toplevelrtti::`RTTI Class Hierarchy Descriptor'": {"toplevelrtti"},
        # Test some complicated grpc cases that required special handling.
        "public: virtual enum grpc_security_level __cdecl grpc_call_credentials::min_security_level(void) const": {
          "grpc_call_credentials"
        },
        "grpc_slice_malloc_large::{lambda(grpc_slice_refcount*)#1}::_FUN(grpc_slice_refcount*)": {
          "grpc_slice_malloc_large"
        },
        # std namespaces are used by get_top_level_namespaces but not by add_automatic_namespaces
        "std::dont_use_this<std::other_thing>()": {"std"},
        "std::vector<inside_std_template_param::class>()": { "std", "inside_std_template_param" }
    }
    all_namespaces = set()
    for (symbol, namespaces) in test_symbols.items():
      self.assertEqual(
          merge_libraries.get_top_level_namespaces(set({symbol})),
          set(namespaces))
      # Gather all the namespaces for later.
      all_namespaces.update(namespaces)
    # Check that if we pass in all the symbols, we get all the namespaces.
    self.assertEqual(
        merge_libraries.get_top_level_namespaces(set(test_symbols.keys())),
        all_namespaces)

    # std:: should not be added as an automatic namespace
    all_namespaces.remove("std")
    prev_flag = FLAGS.hide_cpp_namespaces
    merge_libraries.add_automatic_namespaces(set(test_symbols.keys()))
    self.assertEqual(set(FLAGS.hide_cpp_namespaces), set(all_namespaces))
    FLAGS.hide_cpp_namespaces = prev_flag

  def test_demanglers(self):
    """Verify that Demangler works in both streaming and non-streaming mode."""

    merge_libraries.FLAGS.streaming_demanglers = False
    demangler_list = merge_libraries.init_demanglers()
    for demangler in demangler_list:
      logging.info("Testing %s", os.path.basename(demangler.cmdline[0]))
      self.assertEqual(
          demangler.demangle("regular_c_symbol_nonstreaming"),
          "regular_c_symbol_nonstreaming")
      self.assertEqual(
          demangler.demangle("_ZN12my_namespace19MyClassNonStreamingC2Ev"),
          "my_namespace::MyClassNonStreaming::MyClassNonStreaming()")
    merge_libraries.shutdown_demanglers()

    merge_libraries.FLAGS.streaming_demanglers = True
    demangler_list = merge_libraries.init_demanglers()
    for demangler in demangler_list:
      logging.info("Testing %s streaming", os.path.basename(demangler.cmdline[0]))
      self.assertEqual(
          demangler.demangle("regular_c_symbol_streaming"),
          "regular_c_symbol_streaming")
      self.assertEqual(
          demangler.demangle("_ZN12my_namespace16MyClassStreamingC2Ev"),
          "my_namespace::MyClassStreaming::MyClassStreaming()")
    merge_libraries.shutdown_demanglers()

  def test_extracting_archive(self):
    tempdir = tempfile.mkdtemp()
    cwd = os.getcwd()
    os.chdir(tempfile.mkdtemp())
    try:
      archive = _TEST_FILE
      (obj_file_list, errors) = merge_libraries.extract_archive(archive)
      self.assertEmpty(errors)
      self.assertLen(obj_file_list, 1)
      self.assertTrue(os.path.isfile(obj_file_list[0]))
    finally:
      os.chdir(cwd)
      if os.path.exists(tempdir):
        shutil.rmtree(tempdir)

  def test_error_extracting_archive(self):
    v = logging.get_verbosity()
    # Temporarily turn off warnings so loading a bad filename doesn't log a bunch
    logging.set_verbosity(logging.ERROR)
    (obj_file_list, errors) = merge_libraries.extract_archive("bad_filename.a")
    logging.set_verbosity(v)
    self.assertEmpty(obj_file_list)
    self.assertNotEmpty(errors)

  def test_renaming_c_symbols(self):
    """Verify that C symbols can be renamed."""
    tempdir = tempfile.mkdtemp()
    cwd = os.getcwd()
    os.chdir(tempdir)
    prefix = '_' if merge_libraries.FLAGS.platform == 'darwin' else ''
    try:
      archive = _TEST_FILE

      all_expected_c_symbols = {
          prefix+"global_c_symbol",
          prefix+"test_another_symbol",
          prefix+"test_symbol",
          prefix+"test_yet_one_more_symbol",
      }
      rename_symbols = {prefix+"test_symbol": prefix+"renamed_test_symbol"}
      redefinition_file = merge_libraries.create_symbol_redefinition_file(
          rename_symbols)

      # Extract the .o file from the library.
      (obj_file_list, errors) = merge_libraries.extract_archive(archive)
      self.assertEmpty(errors)
      self.assertLen(obj_file_list, 1)
      self.assertTrue(os.path.isfile(obj_file_list[0]))
      library_file = obj_file_list[0]
      before_symbols = merge_libraries.read_symbols_from_archive(
          library_file)[0]
      before_c_symbols = {s for s in before_symbols
                          if not merge_libraries.is_cpp_symbol(s)}

      self.assertTrue(all_expected_c_symbols.issubset(before_c_symbols))

      expect_c_symbols_after_rename = all_expected_c_symbols
      expect_c_symbols_after_rename.remove(prefix+"test_symbol")
      expect_c_symbols_after_rename.add(prefix+"renamed_test_symbol")

      # Rename the symbols.
      merge_libraries.move_object_file(library_file,
                                       "%s_renamed.o" % library_file,
                                       redefinition_file.name)
      after_symbols = merge_libraries.read_symbols_from_archive("%s_renamed.o" %
                                                                library_file)[0]
      after_c_symbols = {s for s in after_symbols
                         if not merge_libraries.is_cpp_symbol(s)}
      self.assertTrue(expect_c_symbols_after_rename.issubset(after_c_symbols))
    finally:
      os.chdir(cwd)
      if os.path.exists(tempdir):
        shutil.rmtree(tempdir)
  
  def test_renaming_cpp_symbols(self):
    """Verify that C++ symbols can be renamed."""

    merge_libraries.init_cache()  # cache is required for demangling
    merge_libraries.init_demanglers()
    # Make sure the demangler is operating correctly.
    self.assertEqual(
        merge_libraries.demangle_symbol("_Znwm"), "operator new(unsigned long)")
    tempdir = tempfile.mkdtemp()
    cwd = os.getcwd()
    os.chdir(tempdir)
    try:
      archive = _TEST_FILE
      merge_libraries.FLAGS.hide_cpp_namespaces = ["test_namespace"]
      merge_libraries.FLAGS.rename_string = "renamed_"

      # Unlike the c test above, we can't make an exhaustive list of all
      # symbols, so instead, grab all the symbols and make sure that
      # test_namespace is changed to renamed_test_namespace.

      # Extract the .o file from the library.
      (obj_file_list, errors) = merge_libraries.extract_archive(archive)
      self.assertEmpty(errors)
      self.assertLen(obj_file_list, 1)
      self.assertTrue(os.path.isfile(obj_file_list[0]))
      library_file = obj_file_list[0]
      before_symbols = merge_libraries.read_symbols_from_archive(
          library_file)[1]  # read all symbols, not just defined ones
      before_cpp_symbols_raw = {
          s for s in before_symbols if merge_libraries.is_cpp_symbol(s)
      }
      # demangle the symbols for human consumption (and so we can check if the
      # namespace was modified)
      before_cpp_symbols = {
          merge_libraries.demangle_symbol(s) for s in before_cpp_symbols_raw
      }

      # Ensure a few known symbols are present.
      self.assertIn("test_namespace::TestClass::TestClass()",
                    before_cpp_symbols)
      self.assertIn("test_namespace::TestClass::~TestClass()",
                    before_cpp_symbols)
      self.assertIn(
          "test_namespace::TestClass::TestClass(test_namespace::TestClass const&)",
          before_cpp_symbols)
      self.assertIn("test_namespace::TestClass::TestStaticMethod()",
                    before_cpp_symbols)
      self.assertIn(
          "test_namespace::TestClass::TestStaticMethodNotInThisFile()",
          before_cpp_symbols)

      rename_symbols = {}
      for sym in before_cpp_symbols_raw:
        rename_symbols.update(merge_libraries.rename_symbol(sym))

      redefinition_file = merge_libraries.create_symbol_redefinition_file(
          rename_symbols)

      # If no file is written, no symbols were set to be renamed.
      self.assertIsNotNone(redefinition_file)

      # To get the expected list of symbols, simply change test_namespace:: to
      # renamed_test_namespace::
      expect_cpp_symbols_after_rename = set()
      
      for s in before_cpp_symbols:
        if not s.startswith("another_namespace"):
          s = s.replace("test_namespace::", "renamed_test_namespace::")
        expect_cpp_symbols_after_rename.add(s)

      # Rename the symbols.
      merge_libraries.move_object_file(library_file,
                                       "%s_renamed.o" % library_file,
                                       redefinition_file.name)
      after_symbols = merge_libraries.read_symbols_from_archive("%s_renamed.o" %
                                                                library_file)[1]
      after_cpp_symbols_raw = {
          s for s in after_symbols if merge_libraries.is_cpp_symbol(s)
      }
      after_cpp_symbols = {
          merge_libraries.demangle_symbol(s) for s in after_cpp_symbols_raw
      }
      self.assertEqual(after_cpp_symbols, expect_cpp_symbols_after_rename)

      # Ensure the DontRenameThis class isn't renamed, as it's inside an inner namespace.
      self.assertTrue("another_namespace::test_namespace::DontRenameThis::DontRenameThisMethod()" in before_cpp_symbols)
      self.assertTrue("another_namespace::test_namespace::DontRenameThis::DontRenameThisMethod()" in after_cpp_symbols)
    finally:
      merge_libraries.shutdown_demanglers()
      merge_libraries.shutdown_cache()
      os.chdir(cwd)
      if os.path.exists(tempdir):
        shutil.rmtree(tempdir)


if __name__ == "__main__":
  absltest.main()
