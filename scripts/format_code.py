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

from absl import app
from absl import flags

# Flag Definitions:
FLAGS = flags.FLAGS

flags.DEFINE_boolean("git_diff", False, "Use git-diff to assemble a file list")
flags.DEFINE_string("git_range", "main..", "the range string when using "
  "git-diff.")
flags.DEFINE_multi_string("f", None, "Append the filename to the list of "
  "files to check.")
flags.DEFINE_multi_string("d", None, 
  "Append the directory to the file list to format.")
flags.DEFINE_multi_string("r", None, 
  "When formatting  a directory, recurse through its children.")
flags.DEFINE_boolean("format_file", True, "Format files in place.")
flags.DEFINE_boolean("verbose", False, "Execute in verbose mode.")

# Constants:
# The list of file types to run clang-format on.   Used to filter out
# results when searching across directories or git diffs.
FILE_TYPE_EXTENSIONS = (".cpp", ".cc", ".c", ".h")

# Functions:    
def does_file_need_formatting(filename):
  """Executes clang-format on the file to determine if it includes any
  formatting changes the user needs to fix.
    
  Returns:
    True if the file requires format changes, False if formatting would produce
    an identical file.
  """
  args = ['clang-format', '-style=file', '-output-replacements-xml', filename]
  proc = subprocess.Popen(args, stdout=subprocess.PIPE)
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
  args = ['clang-format', '-style=file', '-i', filename]
  proc = subprocess.Popen(args)

def git_diff_list_files():
  """Compares the current branch to master to assemble a list of source
  files which have been altered.
  
  Returns:
    A list of file paths for each file in the git diff list with an extension
    matching one of those in FILE_TYPE_EXTENSIONS.
  """
  # The ACMRT filter lists files that have been (A)dded (C)opied (M)odified
  # (R)enamed or (T)ype changed.
  args = ['git', 'diff', FLAGS.git_range, '--name-only', '--diff-filter=ACMRT']
  filenames = []
  if FLAGS.verbose:
    print()
    print('Executing git diff to detect changed files, git_range: "{0}"'
        .format(FLAGS.git_range))
  proc = subprocess.Popen(args, stdout=subprocess.PIPE)
  for line in io.TextIOWrapper(proc.stdout, encoding="utf-8"):
    line = line.rstrip()
    if(line.endswith(FILE_TYPE_EXTENSIONS)):
      filenames.append(line.strip())
      if FLAGS.verbose:
        print('  - File detected: {0}'.format(line))
  if FLAGS.verbose:
    print('  > Found {0} file(s)'.format(len(filenames)))
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
          print('  - {0}'.format(full_path))
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
  if FLAGS.d:
    for directory in FLAGS.d:
      print('Searching files in directory: "{0}"'.format(directory))
      filenames += list_files_from_directory(directory, FLAGS.r)
  if FLAGS.verbose:
    print('  > Found {0} file(s)'.format(len(filenames)))
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
  if not FLAGS.git_diff and not Flags.f and not Flags.d_:
      print()
      print('ERROR:  -git_diff not defined, and there are no file or')
      print('directory search targets.')
      print('Nothing to do. Exiting.')
      print()
      return False
  return True

def main(argv):
  if not validate_arguments():
    if FLAGS.verbose:
        print("Returning exit code 2")
    sys.exit(2)
  
  filenames = []
  if FLAGS.f:
      filenames = FLAGS.f

  if FLAGS.d:
    filenames += directory_search_list_files()

  if FLAGS.git_diff:
    filenames += git_diff_list_files()

  print('Checking the format of {0} file(s). Please wait:'
    .format(len(filenames)))
  exit_code = 0
  if FLAGS.format_file:
    count = 0
    for filename in filenames:
        if does_file_need_formatting(filename):
          print('  - Formatting: "{0}"'.format(filename))
          format_file(filename)
          count += 1
        else:
           print('  - File already formatted: "{0}"'.format(filename))
    print('   - Formatted {0} files.'.format(count))
  else:
    count = 0
    for filename in filenames:      
      if does_file_need_formatting(filename):  
        exit_code = 1
        count += 1
        print('  - Requires reformatting: "{0}"'.format(filename))
    print('  > Done. {0} file(s) need formatting.'.format(count))
  print()
  if exit_code == 0:
      print('Success!')
  else:
    print('Returning exit code: {0}'.format(exit_code))
  sys.exit(exit_code)

if __name__ == '__main__':
  app.run(main)
