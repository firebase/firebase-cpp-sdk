import os
import sys
import re
import tempfile
import subprocess
import argparse
import functools

import utils

LIBRARY_EXTENSIONS = set(('.lib', '.a'))
MSVC_RUNTIME_LIBRARIES_MAP = {
        'MSVCRT': '/MD',
        'MSVCRTD': '/MDd',
        'LIBCMT': '/MT',
        'LIBCMTD': '/MTd'
        }

def get_libraries_to_inspect(paths, firebase_only=True):
  libraries = []
  for path in paths:
    if os.path.isdir(path):
      for (dirpath, _, filenames) in os.walk(path):
        for filename in filenames:
          _name, ext = os.path.splitext(filename)
          if ext in LIBRARY_EXTENSIONS:
            if firebase_only and 'firebase_' not in filename:
              continue
            libraries.append(os.path.join(dirpath, filename))
    else:
      abspath = os.path.abspath(path)
      if os.path.exists(abspath):
        name, ext = os.path.splitext(path)
        if ext in LIBRARY_EXTENSIONS:
          if firebase_only and 'firebase_' not in name:
            continue
          libraries.append(abspath)
  return libraries

def get_dumpbin_exe_path():
  tempdir = tempfile.gettempdir()
  tempfile_path = os.path.join(tempdir, 'vswhere.exe')

  # Download vswhere tool to determine the install location of Visual Studio on machine.
  utils.download_file('https://github.com/microsoft/vswhere/releases/download/2.8.4/vswhere.exe', tempfile_path)

  output = utils.run_command([tempfile_path, '-latest', '-property', 'installationPath'],
                             capture_output=True, print_cmd=False)

  msvc_dir = os.path.join(output.stdout.replace('\n', ''), 'VC', 'Tools', 'MSVC')
  version_dir = os.listdir(msvc_dir)[0]
  return  os.path.join(msvc_dir, version_dir, 'bin', 'Hostx64' ,'x64', 'dumpbin.exe')

def get_msvc_runtime_linkage_re_pattern():
  return re.compile('.*/DEFAULTLIB:(\w*)')

def get_msvc_runtime_linkage(lib, dumpbin_exe_path, re_pattern=None):
  dumpbin = subprocess.Popen([dumpbin_exe_path, '/directives', lib], stdout=subprocess.PIPE)
  grep = subprocess.Popen(['findstr', '\/DEFAULTLIB'], stdin=dumpbin.stdout, stdout=subprocess.PIPE)
  output = grep.communicate()[0]
  if not output:
      return
  output = output.decode('utf-8')
  if re_pattern is None:
    re_pattern = get_msvc_runtime_linkage_re_pattern()
  runtime_libs = set()
  for line in output.splitlines():
    match = re_pattern.match(line)
    if match:
        runtime_lib = match.groups()[0]
        runtime_libs.add(runtime_lib)

  msvc_runtime_libs = set( MSVC_RUNTIME_LIBRARIES_MAP.keys()).intersection(runtime_libs)
  if len(msvc_runtime_libs) != 1:
      return 'N.A'

  return MSVC_RUNTIME_LIBRARIES_MAP.get(msvc_runtime_libs.pop(), 'N.A')

def get_arch_re_pattern_windows():
  return re.compile('.* machine \((\w*)\)')

def get_arch_windows(lib, dumpbin_exe_path, re_pattern=None):
  dumpbin = subprocess.Popen([dumpbin_exe_path, '/headers', lib], stdout=subprocess.PIPE)
  grep = subprocess.Popen(['findstr', 'machine'], stdin=dumpbin.stdout, stdout=subprocess.PIPE)
  output = grep.communicate()[0]
  if not output:
      return
  output = output.decode('utf-8')
  if re_pattern is None:
    re_pattern = get_arch_re_pattern_windows()
  archs = set()
  for line in output.splitlines():
    match = re_pattern.match(line)
    if match:
        arch = match.groups()[0]
        archs.add(arch)

  if len(archs)!=1:
      return 'N.A'
  return archs.pop()

def get_arch_re_pattern_linux():
    return re.compile('architecture: (\w*), \w*')

def get_arch_linux(lib, re_pattern=None):
  if not utils.is_linux_os():
    print("ERROR: get_arch_linux function can only be called on macs. Exiting ")
    sys.exit(1)

  if re_pattern is None:
    re_pattern = get_arch_re_pattern_linux()

  output = utils.run_command(['objdump', '-f', lib], capture_output=True, print_cmd=False)
  lines = output.stdout.splitlines()
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

  return object_files

def get_arch_re_pattern_mac():
    return re.compile('.* is architecture: (\w*)')

def get_arch_mac(lib, re_pattern=None):
  if not utils.is_mac_os():
    print("ERROR: get_arch_mac function can only be called on macs. Exiting ")
    sys.exit(1)

  output = utils.run_command(['lipo', '-info', lib], capture_output=True, print_cmd=False)
  if re_pattern is None:
    re_pattern = get_arch_re_pattern_mac()

  match = re_pattern.match(output.stdout)
  if not match:
    return
  return match.groups()[0]


def main():
  args = parse_cmdline_args()

  if utils.is_linux_os():
    pattern = get_arch_re_pattern_linux()
    arch_function = functools.paget_arch_linux
  elif utils.is_mac_os():
    pattern = get_arch_re_pattern_mac()
    arch_function = get_arch_mac
  else:
    pattern = get_arch_re_pattern_windows()
    dumpbin = get_dumpbin_exe_path()
    arch_function = get_arch_windows

  libs = get_libraries_to_inspect(args.libraries, not args.all)
  for lib in libs:
    print(lib)
    # dumpbin = get_dumpbin_exe_path()
    # arch = get_arch_windows(lib, dumpbin)
    # rl = get_msvc_runtime_linkage(lib, dumpbin)
    arch = get_arch_mac(lib)
    print(arch)

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Inspect prebuilt libraries/archives '
                                               'for architecture (x64, x86) '\
                                               'and msvc runtime linkage (/MD vs /MT).')
  parser.add_argument('libraries', metavar='L', nargs='+',
                    help='List of files (libraries) and/or directories (containing libraries).')
  parser.add_argument('--all', action='store_true',
                      help='Inspect all libraries found under specified directory.')
  parser.add_argument('--print_full_paths', action='store_true',
                      help='Print full library paths in summary.')
  parser.add_argument('--verbose', action='store_true',
                      help='Print all relevant output as it is (without summarizing).')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()
