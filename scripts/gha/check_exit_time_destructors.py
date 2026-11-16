#!/usr/bin/env python

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
"""Checks that Firebase static libraries register no exit-time destructors.

A static or global C++ object with a non-trivial destructor makes the compiler
register that destructor with `__cxa_atexit` (or `atexit`). SDK state destroyed
that way can be used afterwards by user-owned static Firebase objects, so the
SDK keeps such state in `firebase::NoDestructor` (app/src/no_destructor.h).

Clang builds enforce this with -Werror=exit-time-destructors (see
cmake/exit_time_destructors.cmake). This script enforces it for compilers
without that diagnostic, such as GCC, by looking for object files that
reference the registration functions.

Usage, from the root of the repo, after building into build/:

$ python3 scripts/gha/check_exit_time_destructors.py build/
"""

import argparse
import os
import re
import subprocess
import sys

# Libraries that must not register exit-time destructors. firebase_firestore is
# not listed: it includes firebase-ios-sdk headers that declare function-local
# statics with destructors.
CHECKED_LIBRARIES = (
    'libfirebase_analytics.a',
    'libfirebase_app.a',
    'libfirebase_app_check.a',
    'libfirebase_auth.a',
    'libfirebase_database.a',
    'libfirebase_functions.a',
    'libfirebase_installations.a',
    'libfirebase_messaging.a',
    'libfirebase_remote_config.a',
    'libfirebase_rest_lib.a',
    'libfirebase_storage.a',
    'libfirebase_ump.a',
)

# Undefined symbols that register a destructor to run at exit. Mach-O prefixes
# C symbols with an extra underscore.
REGISTRATION_SYMBOL = re.compile(r'^_?(__cxa_atexit|atexit)$')


def find_libraries(build_dir):
  """Finds the checked libraries under a build directory.

  Args:
    build_dir (str): Directory to search recursively.

  Returns:
    dict(str, str): Path of each library that was found, keyed by file name.
  """
  found = {}
  for dirpath, _, filenames in os.walk(build_dir):
    for filename in filenames:
      if filename in CHECKED_LIBRARIES:
        found.setdefault(filename, os.path.join(dirpath, filename))
  return found


def find_offending_objects(library, nm):
  """Lists the object files in a library that register exit-time destructors.

  Args:
    library (str): Path to a static library.
    nm (str): The nm executable to run.

  Returns:
    list(str): "object: symbol" for each offending reference.
  """
  # -A prefixes each line with "library:object:", -u lists undefined symbols.
  output = subprocess.run([nm, '-A', '-u', library], check=True,
                          stdout=subprocess.PIPE, universal_newlines=True).stdout
  offenders = []
  for line in output.splitlines():
    fields = line.split()
    if not fields or not REGISTRATION_SYMBOL.match(fields[-1]):
      continue
    object_file = fields[0].rstrip(':').split(':')[-1]
    offenders.append('%s: %s' % (object_file, fields[-1]))
  return offenders


def main():
  args = parse_cmdline_args()
  libraries = find_libraries(args.build_dir)
  missing = sorted(set(CHECKED_LIBRARIES) - set(libraries))
  if missing and not args.allow_missing:
    print('Libraries not found under %s: %s' % (args.build_dir,
                                                ', '.join(missing)))
    return 1

  failed = False
  for name in sorted(libraries):
    offenders = find_offending_objects(libraries[name], args.nm)
    if not offenders:
      print('OK: %s' % libraries[name])
      continue
    failed = True
    print('FAILED: %s registers exit-time destructors:' % libraries[name])
    for offender in offenders:
      print('  %s' % offender)
  if failed:
    print('Wrap the static objects defined in the files above in '
          'firebase::NoDestructor (app/src/no_destructor.h).')
  return 1 if failed else 0


def parse_cmdline_args():
  parser = argparse.ArgumentParser(
      description='Check Firebase static libraries for exit-time destructors.')
  parser.add_argument('build_dir',
                      help='Build directory to search for libraries.')
  parser.add_argument('--nm', default='nm', help='The nm executable to use.')
  parser.add_argument('--allow_missing', action='store_true',
                      help='Do not fail when a checked library was not built.')
  return parser.parse_args()


if __name__ == '__main__':
  sys.exit(main())
