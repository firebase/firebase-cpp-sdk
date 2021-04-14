#!/usr/bin/env python3

# Copyright 2021 Google LLC
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

"""
A tool for manage source file formatting.

Assembles a list of files via paths, directories, and/or git diffs and runs
clang-format on them.  By default the tool will format the files in place.
-noformat_file will run a check-only mode.

Returns:
   0: Either no files were found which need formatting, or if format_file was
     enablled and the files were succesfully formatted.
   1: Files which need formatting were found but weren't formatted
     because -noformat_file was set.
   2: The application wasn't configured properly and could not execute.
"""
import io
import os
import subprocess
import sys

from os import listdir
from os.path import isfile, join
from absl import app
from absl import flags


# Flag Definitions:

FLAGS = flags.FLAGS

flags.DEFINE_boolean("git_diff", False, "Use git-diff to assemble a file list")
flags.DEFINE_string("git_range", "main..", "the range string when using "
  "git-diff")
flags.DEFINE_multi_string("file", None, "Append the filename to the list of "
  "files to check.")
flags.DEFINE_multi_string("d", None, 
  "Append the directory to the file list to format.")
flags.DEFINE_multi_string("dr", None, 
  "Append the directory, and recurse through its children, and add their "
  "contents to the list of files to check.")
flags.DEFINE_boolean("format_file", True, "Format files in place")
flags.DEFINE_boolean("verbose", False, "Execute in verbose mode")

# Constants:

# The list of file types to run clang-format on.   Used to filter out
# results when searching across directories or git diffs.
FILE_TYPE_EXTENSIONS = (".ccp", ".cc", ".c", ".h")

# Common execution arguments across all clang-format executions.
CLANG_FORMAT_BASE = "clang-format -style=file "

# Flag which informs clang-format to alter the file in place, overwriting
# the original file with thew newly formatted version.
CLANG_FORMAT_PARM_IN_PLACE = "-i "

# Flag which informs clang-format to use stdout to log the changes it would
# make to a file.  Logging in XML format allows us to easily parse the results 
# to detect if the file requires formatting changes.
CLANG_FORMAT_PARAM_LOG_REPLACEMENTS = "-output-replacements-xml "

# Conmmon exeuction arguments across all git diff executions.
GIT_DIFF_BASE = "git diff "

# Parameters to append for all git diff executions. The ACMRT filter lists
# the files that have been (A)dded (C)opied (M)odified (R)enamed or (T)ype
# changed.
GIT_DIFF_PARAMS = " --name-only --diff-filter=ACMRT"

# Functions:    

def does_file_need_formatting(filename):
  """Executes clang-format on the file to determine if it includes any
  formatting changes the user needs to fix.
    
  Returns:
    True if the file requires format changes, False if formatting would produce
    an identical file.
  """
  command_line = CLANG_FORMAT_BASE + CLANG_FORMAT_PARAM_LOG_REPLACEMENTS
  command_line += filename
  args = command_line.split(" ")
  proc = subprocess.Popen(command_line.split(" "), stdout=subprocess.PIPE)
  for line in io.TextIOWrapper(proc.stdout, encoding="utf-8"):
    if line.strip().startswith("<replacement "):
      return True
  return False    

def format_file(filename):
  """Executes clang-format on the file to reformat it according to our
  project's standards.  If -format_in_place is defined, the file will be
  formatted in place, with the results overwriting the previous version
  of the file. Otherwise the result of the file reformat will be logged
  to standard out.

  Args:
   filename: path to the file to format.
  """
  command_line = CLANG_FORMAT_BASE + CLANG_FORMAT_PARM_IN_PLACE + filename
  args = command_line.split(" ")
  proc = subprocess.Popen(args)

def git_diff_list_files():
  """Compares the current branch to master to assemble a list of source
  files which have been altered.
  
  Returns:
    A list of file paths for each file in the git diff list with an extension
    matching one of those in FILE_TYPE_EXTENSIONS.
  """
  filenames = []
  command_line = (GIT_DIFF_BASE + FLAGS.git_range + GIT_DIFF_PARAMS).strip()
  args = command_line.split(" ")
  if FLAGS.verbose:
    print()
    print("Executing git diff to detect changed files, git_range:",
      "\"" + FLAGS.git_range + "\"") 
  proc = subprocess.Popen(args, stdout=subprocess.PIPE)
  for line in io.TextIOWrapper(proc.stdout, encoding="utf-8"):
    line = line.rstrip()
    if(line.endswith(FILE_TYPE_EXTENSIONS)):
      filenames.append(line.strip())
      if FLAGS.verbose:
        print("  - File detected: " + line)
  if filenames and FLAGS.verbose:
    print()
  return filenames

def list_files_from_directory(path, recurse):
  """Iterates through the the directory and returns a list of files
  which match those with extensions defined in FILE_TYPE_EXTENSIONS. 

  Args:
    path: the path to a directory to start searching from.
    recurse: when True, will also recursively search for files in any
      subdirectories found in path.
  
  Returns:
    A list of matching filenames found in the directory.
  """
  filenames = []
  for root, dirs, files in os.walk(path):
    path = root.split(os.sep)
    for filename in files:
      if filename.endswith(FILE_TYPE_EXTENSIONS):
        full_path = os.path.join(root, filename)
        if FLAGS.verbose:
          print("\t\t" + full_path)
        filenames.append(full_path)  
      if not recurse:
        break;
  return filenames

def directory_search_list_files():
  """Create a list of files based on the directories defined in the command
  line arguments -dr and -d.
  
  Returns:
    A list of filenames which match one of the extensions in
    FILE_TYPE_EXTENSIONS.
  """
  filenames = []
  if FLAGS.dr:
    if FLAGS.verbose:
      print()
      print("Adding files by recursivly traversing directories:")
    for directory in FLAGS.dr:
      if FLAGS.verbose:
        print("\tRecursive searching directory: \""+directory+"\"")
      filenames += list_files_from_directory(directory, True)
  if FLAGS.d:
    for directory in FLAGS.d:
      print("\Searching files in directory: \""+directory+"\"")
      filenames += list_files_from_directory(directory, False)
  if FLAGS.verbose:
    print("  Found", len(filenames), "files.")
    print()
  return filenames

def validate_arguments():
  """Ensures that a proper command line configuration exists to create a file
  list either via git-diff operations or via manuallyd defined --file lists.
  Logs an error if theres an errant configuration.
  
  Returns:
    True if the either directories or git diff is defined, or both.  Returns
    False otherwise signallingt hat execution should end.
  """
  if not FLAGS.git_diff:
    if not FLAGS.file: 
      print()
      print("ERROR:  -git_diff not defined, and there are no -file targets.")
      print("Nothing to do. Exiting.")
      print()
      return False
  return True

def main(argv):
  if not validate_arguments():
    if FLAGS.verbose:
        print("Returning exit code 2")
    sys.exit(2)
  
  filenames = []

  if FLAGS.dr or FLAGS.d:
    filenames += directory_search_list_files()

  if FLAGS.git_diff:
    filenames += git_diff_list_files()

  print("Checking the format of", len(filenames), "file(s). Please wait:")
  exit_code = 0
  if FLAGS.format_file:
    count = 0
    for filename in filenames:
        if does_file_need_formatting(filename):
          print("  - Formatting: \""+filename+"\"")
          format_file(filename)
          count += 1
        else:
           print("  - File already formatted: \""+filename+"\"")
    print("  - Formatted", count, "files.")
  else:
    count = 0
    for filename in filenames:      
      if does_file_need_formatting(filename):  
        exit_code = 1
        count += 1
        print("  - Requires reformatting:", "\""+filename+"\"")
    print("  - Done. ", count, " files need formatting.")
  print()
  if exit_code == 0:
      print("Success!")
  else:
    print("Returning exit code:", exit_code)
  sys.exit(exit_code)

if __name__ == '__main__':
  app.run(main)
