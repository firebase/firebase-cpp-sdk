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
import difflib
import io
import os
import subprocess
import sys

from absl import app
from absl import flags

# Flag Definitions:
FLAGS = flags.FLAGS

flags.DEFINE_boolean('git_diff', False, 'Use git-diff to assemble a file list')
flags.DEFINE_string('git_range', 'origin/main..', 'the range string when using '
  'git-diff.')
flags.DEFINE_multi_string('f', None, 'Append the filename to the list of '
  'files to check.')
flags.DEFINE_multi_string('d', None,
  'Append the directory to the file list to format.')
flags.DEFINE_boolean('r', False,
  "Recurse through the directory's children When formatting a target directory.")
flags.DEFINE_boolean('format_file', True, 'Format files in place.')
flags.DEFINE_boolean("verbose", False, 'Execute in verbose mode.')
flags.DEFINE_boolean("github_log", False, 'Pring special github log format items.')

# Constants:
FILE_TYPE_EXTENSIONS = ('.cpp', '.cc', '.c', '.h', '.m', '.mm', '.java')
"""Tuple: The file types to run clang-format on.
Used to filter out results when searching across directories or git diffs.
"""

# Functions:
def get_formatting_diff_lines(filename):
  """Calculates and returns a printable diff of the formatting changes that
  would be applied to the given file by clang-format.

  Args:
    filename (string): path to the file whose formatting diff to calculate.

  Returns:
    Iterable[str]: The diff of the formatted file against the original file;
      each string in the returned iterable is a "line" of the diff; they could
      be printed individually or joined into a single string using something
      like os.linesep.join(diff_lines), where `diff_lines` is the return value.
  """
  args = ['clang-format', '-style=file', filename]
  result = subprocess.run(args, stdout=subprocess.PIPE, check=True)

  formatted_lines = [line.rstrip('\r\n')
      for line in result.stdout.decode('utf8', errors='replace').splitlines()]
  with open(filename, 'rt', encoding='utf8', errors='replace') as f:
    original_lines = [line.rstrip('\r\n') for line in f]

  return [line.rstrip()
      for line in difflib.unified_diff(original_lines, formatted_lines)]

def does_file_need_formatting(filename):
  """Executes clang-format on the file to determine if it includes any
  formatting changes the user needs to fix.
  Args:
    filename (string): path to the file to check.

  Returns:
    bool: True if the file requires format changes, False if formatting would produce
    an identical file.
  """
  args = ['clang-format', '-style=file', '-output-replacements-xml', filename]
  result = subprocess.run(args, stdout=subprocess.PIPE, check=True)
  for line in result.stdout.decode('utf-8').splitlines():
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
   filename (string): path to the file to format.
  """
  args = ['clang-format', '-style=file', '-i', filename]
  subprocess.run(args, check=True)

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
  result = subprocess.run(args, stdout=subprocess.PIPE)
  for line in result.stdout.decode('utf-8').splitlines():
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
    path (string): the path to a directory to start searching from.
    recurse (bool): when True, will also recursively search for files in any
      subdirectories found in path.
  
  Returns:
    list: the filenames found in the directory which match one of the extensions
    in FILE_TYPE_EXTENSIONS.
  """
  filenames = []
  for root, dirs, files in os.walk(path):
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
    list: the filenames which match one of the extensions in
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
    bool: True if the either directories or git diff is defined, or both.
    Returns False otherwise signalling that execution should end.
  """
  if not FLAGS.git_diff and not FLAGS.f and not FLAGS.d:
      print()
      print('ERROR:  -git_diff not defined, and there are no file or')
      print('directory search targets.')
      print('Nothing to do. Exiting.')
      return False
  return True

def main(argv):
  if not validate_arguments():
    sys.exit(2)
  
  filenames = []
  if FLAGS.f:
      filenames = FLAGS.f

  if FLAGS.d:
    filenames += directory_search_list_files()

  if FLAGS.git_diff:
    filenames += git_diff_list_files()

  exit_code = 0
  if not filenames:
    print('No files to format.')
    sys.exit(exit_code)

  if FLAGS.verbose:
    print('Found {0} file(s). Checking their format.'.format(len(filenames)))

  if FLAGS.format_file:
    count = 0
    for filename in filenames:
        if does_file_need_formatting(filename):
          if FLAGS.verbose:
            print('  - FRMT: "{0}"'.format(filename))
          format_file(filename)
          count += 1
        else:
          if FLAGS.verbose:
            print('  - OK:   "{0}"'.format(filename))
    print('  > Formatted {0} file(s).'.format(count))
  else:
    github_log = ['::error ::FILE FORMATTING ERRORS:','']
    count = 0
    for filename in filenames:
      if does_file_need_formatting(filename):
        exit_code = 1
        count += 1
        github_log.append('- Requires reformatting: "{0}"'.format(filename))
        if FLAGS.verbose:
          print('  - Requires reformatting: "{0}"'.format(filename))
          print('------ BEGIN FORMATTING DIFF OF {0}'.format(filename))
          for diff_line in get_formatting_diff_lines(filename):
            print(diff_line)
          print('------ END FORMATTING DIFF OF {0}'.format(filename))
    if FLAGS.verbose:
      print('  > Done. {0} file(s) need formatting.'.format(count))
    else:
      print('{0} file(s) need formatting.'.format(count))
      print('run: scripts/format_code.py -git_diff')
    if exit_code and FLAGS.github_log:
      github_log.append('')
      github_log.append('{0} file(s) need formatting.'.format(count))
      github_log.append('run: scripts/format_code.py -git_diff')
      print('%0A'.join(github_log))

  sys.exit(exit_code)

if __name__ == '__main__':
  app.run(main)
