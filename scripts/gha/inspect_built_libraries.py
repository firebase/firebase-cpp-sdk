#!/usr/bin/env python

# Copyright 2020 Google
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
"""Script to inspect architecture and msvc runtime libraries(on windows) in prebuilt libraries.

This is a cross platform script that can verify the built libraries for expected
cpu architecture and msvc runtime (only on windows).

CPU architecture is either x64 or x86
MSVC runtime library is either static (/MT or /MTd) or dynamic (/MD or /MDd)

Usage:
Ideally the script could be run from anywhere and only depends upon the `utils.py`
module but it is a good practice to run it from the root of the repo.

# Inspect firebase libraries as the default value for --library_filter is "firebase_"
$ python scripts/gha/inspect_built_libraries.py build/
Library                                Architecture
****************************************************
libfirebase_admob.a                    x86_64
libfirebase_analytics.a                x86_64
libfirebase_app.a                      x86_64
libfirebase_auth.a                     x86_64
libfirebase_database.a                 x86_64
libfirebase_dynamic_links.a            x86_64
...

# To print full paths to libraries and specify a different library filter.
$ python scripts/gha/inspect_built_libraries.py build/ --library_filter "grpc" --print_full_paths
************************************************************************************
Library                                                                Architecture
************************************************************************************
build/external/src/firestore-build/external/src/grpc-build/libgrpc++.a x86_64
build/external/src/firestore-build/external/src/grpc-build/libgrpc.a   x86_64

# To print information for all libraries, set library_filter to an empty string.
$ python scripts/gha/inspect_built_libraries.py build/ --library_filter ""

# On Windows, there is an additional column of information for msvc runtime.
$ python scripts/gha/inspect_built_libraries.py build/
******************************************************************
Library                                 Architecture MSVC_Runtime
******************************************************************
firebase_admob-d.lib                    x64          /MTd
firebase_analytics-d.lib                x64          /MTd
firebase_app-d.lib                      x64          /MTd
firebase_auth-d.lib                     x64          /MTd
firebase_database-d.lib                 x64          /MTd
...


"""

import argparse
import functools
import os
import re
import subprocess
import sys
import tempfile

import utils

LIBRARY_EXTENSIONS = set(('.lib', '.a'))
MSVC_RUNTIME_LIBRARIES_MAP = {
        'MSVCRT': '/MD',
        'MSVCRTD': '/MDd',
        'LIBCMT': '/MT',
        'LIBCMTD': '/MTd'
        }

def get_libraries_to_inspect(paths, library_filter=None):
  libraries = []
  for path in paths:
    if os.path.isdir(path):
      for (dirpath, _, filenames) in os.walk(path):
        for filename in filenames:
          _name, ext = os.path.splitext(filename)
          if ext in LIBRARY_EXTENSIONS:
            if library_filter and library_filter not in filename:
              continue
            libraries.append(os.path.join(dirpath, filename))
    else:
      abspath = os.path.abspath(path)
      if os.path.exists(abspath):
        name, ext = os.path.splitext(path)
        if ext in LIBRARY_EXTENSIONS:
          if library_filter and library_filter not in name:
            continue
          libraries.append(abspath)
  return libraries

def print_summary_table(headers, rows):
  colwidths = []
  for idx, _ in enumerate(headers):
    colwidths.append(max( [len(headers[idx])] + [ len(row[idx]) for row in rows])+1)

  print('*'*sum(colwidths))
  header_string = ''
  for idx, header in enumerate(headers):
    header_string = header_string + header.ljust(colwidths[idx])
  print(header_string)
  print('*'*sum(colwidths))

  for row in rows:
    row_string = ''
    for idx, col_item in enumerate(row):
      row_string = row_string + col_item.ljust(colwidths[idx])
    print(row_string)

@functools.lru_cache
def get_or_create_dumpbin_exe_path():
  tempdir = tempfile.gettempdir()
  vswhere_tempfile_path = os.path.join(tempdir, 'vswhere.exe')
  if not os.path.exists(vswhere_tempfile_path):
    # Download vswhere tool to determine the install location of Visual Studio on machine.
    utils.download_file('https://github.com/microsoft/vswhere/releases/download/2.8.4/vswhere.exe',
                        vswhere_tempfile_path)

  output = utils.run_command([vswhere_tempfile_path, '-latest', '-property', 'installationPath'],
                             capture_output=True, print_cmd=False)

  msvc_dir = os.path.join(output.stdout.replace('\n', ''), 'VC', 'Tools', 'MSVC')
  version_dir = os.listdir(msvc_dir)[0]

  # Try to locate dumpbin exe in all possible directories. x64 is preferred.
  tools_archs = ('x64' , 'x86')
  for tools_arch in tools_archs:
    dumpbin_exe_path = os.path.join(msvc_dir, version_dir, 'bin',
                                    'Host'+tools_arch, tools_arch, 'dumpbin.exe')
    if os.path.exists(dumpbin_exe_path):
      return dumpbin_exe_path

  raise ValueError("Could not find dumpbin.exe tool on this Windows machine. "\
                   "It is required to do any inspections or queries on windows libraries/archives.")


def get_msvc_runtime_linkage_re_pattern():
  return re.compile('.*/DEFAULTLIB:(\w*)')


def inspect_msvc_runtime_linkage(lib, dumpbin_exe_path=None):
  if dumpbin_exe_path is None:
    dumpbin_exe_path = get_or_create_dumpbin_exe_path()

  dumpbin = subprocess.Popen([dumpbin_exe_path, '/directives', lib], stdout=subprocess.PIPE)
  grep = subprocess.Popen(['findstr', '\/DEFAULTLIB'], stdin=dumpbin.stdout, stdout=subprocess.PIPE)
  output = grep.communicate()[0]
  if not output:
      return
  return output.decode('utf-8')


def summarize_msvc_runtime_linkage(verbose_data, re_pattern=None):
  if re_pattern is None:
    re_pattern = get_msvc_runtime_linkage_re_pattern()
  runtime_libs = set()
  for line in verbose_data.splitlines():
    match = re_pattern.match(line)
    if match:
        runtime_libs.add(match.groups()[0])

  msvc_runtime_libs = set( MSVC_RUNTIME_LIBRARIES_MAP.keys()).intersection(runtime_libs)
  if len(msvc_runtime_libs) != 1:
      return 'N.A'
  return MSVC_RUNTIME_LIBRARIES_MAP.get(msvc_runtime_libs.pop(), 'N.A')


def get_arch_re_pattern_windows():
  return re.compile('.* machine \((\w*)\)')


def inspect_arch_windows(lib, dumpbin_exe_path=None):
  if not utils.is_windows_os():
    print("ERROR: inspect_arch_windows function can only be called on windows. Exiting ")
    sys.exit(1)
  if dumpbin_exe_path is None:
    dumpbin_exe_path = get_or_create_dumpbin_exe_path()

  dumpbin = subprocess.Popen([dumpbin_exe_path, '/headers', lib], stdout=subprocess.PIPE)
  grep = subprocess.Popen(['findstr', 'machine'], stdin=dumpbin.stdout, stdout=subprocess.PIPE)
  output = grep.communicate()[0]
  if not output:
      return
  return output.decode('utf-8')


def summarize_arch_windows(verbose_data, re_pattern=None):
  if re_pattern is None:
    re_pattern = get_arch_re_pattern_windows()
  archs = set()
  for line in verbose_data.splitlines():
    match = re_pattern.match(line)
    if match:
        arch = match.groups()[0]
        archs.add(arch)
  if len(archs)!=1:
      return 'N.A'
  return archs.pop()

def get_arch_re_pattern_linux():
    return re.compile('architecture: (\w*), \w*')

def inspect_arch_linux(lib):
  if not utils.is_linux_os():
    print("ERROR: inspect_arch_linux function can only be called on linux. Exiting ")
    sys.exit(1)
  objdump = subprocess.Popen(['objdump', '-f', lib], stdout=subprocess.PIPE)
  grep = subprocess.Popen(['grep', '-B', '1', 'architecture'], stdin=objdump.stdout, stdout=subprocess.PIPE)
  output = grep.communicate()[0]
  if not output:
      return
  return output.decode('utf-8')

def summarize_arch_linux(verbose_data, re_pattern=None):
  if re_pattern is None:
    re_pattern = get_arch_re_pattern_linux()
  lines = verbose_data.splitlines()
  object_files = {}
  for idx,line in enumerate(lines):
    if '.o:' in line:
      parts = line.split(':')
      objname = parts[0]
      arch_line = lines[idx+1]
      match = re_pattern.match(arch_line)
      if match:
        arch = match.groups()[0]
        object_files[objname] = arch
  archs = set(object_files.values())
  if len(archs) != 1:
    return 'N.A'
  return archs.pop()

def get_arch_re_pattern_mac():
    return re.compile('.* is architecture: (\w*)')

def inspect_arch_mac(lib):
  if not utils.is_mac_os():
    print("ERROR: inspect_arch_mac function can only be called on macs. Exiting ")
    sys.exit(1)

  output = utils.run_command(['lipo', '-info', lib], capture_output=True, print_cmd=False)
  return output.stdout

def summarize_arch_mac(verbose_data, re_pattern=None):
  if re_pattern is None:
    re_pattern = get_arch_re_pattern_mac()

  match = re_pattern.match(verbose_data)
  if not match:
    return 'N.A'
  return match.groups()[0]

def main():
  args = parse_cmdline_args()

  summary_headers = ['Library', 'Architecture']
  if utils.is_linux_os():
    arch_pattern = get_arch_re_pattern_linux()
    inspect_arch_function = inspect_arch_linux
    summarize_arch_function = functools.partial(summarize_arch_linux,
                                                re_pattern=arch_pattern)
  elif utils.is_mac_os():
    arch_pattern = get_arch_re_pattern_mac()
    inspect_arch_function = inspect_arch_mac
    summarize_arch_function = functools.partial(summarize_arch_mac,
                                                re_pattern=arch_pattern)
  elif utils.is_windows_os():
    arch_pattern = get_arch_re_pattern_windows()
    dumpbin = get_or_create_dumpbin_exe_path()
    inspect_arch_function = functools.partial(inspect_arch_windows,
                                              dumpbin_exe_path=dumpbin)
    summarize_arch_function = functools.partial(summarize_arch_windows,
                                                re_pattern=arch_pattern)
    summary_headers.append('MSVC_Runtime')
    msvc_runtime_linkage_pattern = get_msvc_runtime_linkage_re_pattern()

  else:
    raise ValueError("Unsupported desktop OS. Can be either Mac, Linux or Windows.")

  # We technically dont use regex or glob filters but we still cover the simplest
  # and most frequently used symbol * just incase. Avoiding regex for speed and
  # convenience (users can easily forget quotes around * and that expands to
  # all files in current directory)
  libs = get_libraries_to_inspect(args.libraries,
                                  args.library_filter.replace('*', ''))
  all_libs_info = []
  for lib in libs:
    libname = os.path.basename(lib) if not args.print_full_paths else lib
    libinfo = [libname]
    try:
      verbose_data = inspect_arch_function(lib)
      if args.verbose:
        print("Inspecting architecture: {0}".format(lib))
        print(verbose_data)
        print()
      arch = summarize_arch_function(verbose_data)
      libinfo.append(arch)
    except:
      libinfo.append('N.A')

    if utils.is_windows_os():
      try:
        verbose_data = inspect_msvc_runtime_linkage(lib, dumpbin_exe_path=dumpbin)
        if args.verbose:
          print("Inspecting msvc runtime library: {0}".format(lib))
          print(verbose_data)
          print()
        runtime_linkage = summarize_msvc_runtime_linkage(verbose_data,
                                      re_pattern=msvc_runtime_linkage_pattern)
        libinfo.append(runtime_linkage)
      except:
        libinfo.append('N.A')
    all_libs_info.append(libinfo)

  all_libs_info.sort(key=lambda x:x[0])
  print_summary_table(summary_headers, all_libs_info)

  if args.cleanup:
    if utils.is_windows_os():
      os.remove(dumpbin)


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Inspect prebuilt libraries/archives '
                                               'for architecture (x64, x86) '\
                                               'and msvc runtime linkage (/MD vs /MT).')
  parser.add_argument('libraries', metavar='L', nargs='+',
                    help='List of files (libraries) and/or directories (containing libraries).')
  parser.add_argument('--library_filter', default='firebase_',
                      help='Filter for library names. Pass an empty string for no filtering.')
  parser.add_argument('--all', action='store_true',
                      help='Inspect all libraries found under specified directory.')
  parser.add_argument('--print_full_paths', action='store_true',
                      help='Print full library paths in summary.')
  parser.add_argument('--verbose', action='store_true',
                      help='Print all relevant output as it is (before summarizing).')
  parser.add_argument('--cleanup', action='store_true',
                      help='Cleanup any temporary data created/downloaded')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()
