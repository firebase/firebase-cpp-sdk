import os
import sys
import re
import tempfile
import subprocess

import utils

LIBRARY_EXTENSIONS = set(('.lib', '.a'))
MSVC_RUNTIME_LIBRARIES_MAP = {
        'MSVCRT': '/MD',
        'MSVCRTD': '/MDd',
        'LIBCMT': '/MT',
        'LIBCMTD': '/MTd'
        }

def get_libraries_to_inspect(paths):
  libraries = []
  for path in paths:
    if os.path.isdir(path):
      for (dirpath, _, filenames) in os.walk(path):
        for filename in filenames:
          _name, ext = os.path.splitext(filename)
          if ext in LIBRARY_EXTENSIONS:
            libraries.append(os.path.join(dirpath, filename))
    else:
      abspath = os.path.abspath(path)
      if os.path.exists(abspath):
        _name, ext = os.path.splitext(path)
        if ext in LIBRARY_EXTENSIONS:
          libraries.append(abspath)
    return libraries

def get_dumpbin_exe_path():
  tempdir = tempfile.gettempdir()
  tempfile_path = os.path.join(tempdir, 'vswhere.exe')

  # Download vswhere tool to determine the install location of Visual Studio on machine.
  utils.download_file('https://github.com/microsoft/vswhere/releases/download/2.8.4/vswhere.exe', tempfile_path)

  output = utils.run_command([tempfile_path, '-latest', '-property', 'installationPath'],
                             capture_output=True)

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

  output = utils.run_command(['objdump', '-f', lib], capture_output=True)
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

  output = utils.run_command(['lipo', '-info', lib], capture_output=True)
  if re_pattern is None:
    re_pattern = get_arch_re_pattern_mac()

  match = re_pattern.match(output.stdout)
  if not match:
    return
  return match.groups()[0]


def main():
  libs = get_libraries_to_inspect([sys.argv[1]])
  for lib in libs:
    print(lib)
    dumpbin = get_dumpbin_exe_path()
    arch = get_arch_windows(lib, dumpbin)
    rl = get_msvc_runtime_linkage(lib, dumpbin)
    print(arch)
    print(rl)


if __name__ == '__main__':
  main()
