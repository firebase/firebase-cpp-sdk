import os
import sys
import re
import tempfile

import utils

LIBRARY_EXTENSIONS = set(('.lib', '.a'))

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

def get_dumpbin_exe_path(vsroot):
  tempdir = tempfile.gettempdir()
  tempfile_path = os.path.join(tempdir, 'vswhere.exe')

  # Download vswhere tool to determine the install location of Visual Studio on machine.
  utils.download_file('https://github.com/microsoft/vswhere/releases/download/2.8.4/vswhere.exe', tempfile_path)

  output = utils.run_command([tempfile_path, '-latest', '-property', 'installationPath'],
                             capture_output=True)

  msvc_dir = os.path.join(output.stdout, 'VC', 'Tools', 'MSVC')
  version_dir = os.listdir(msvc_dir)
  return  os.path.join(msvc_dir, version_dir, 'Hostx64' ,'x64', 'dumpbin.exe')


def get_arch_windows(lib, dumpbin_exe_path, re_pattern=None):
  output = utils.run_command([dumpbin_exe_path, '/headers', lib], capture_output=True)
  print output

def get_arch_re_pattern_linux():
    return re.compile('architecture: (.*), .*')

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
    return re.compile('.* is architecture: (.*)')

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
    arch = get_arch_mac(lib)
    print(arch)


if __name__ == '__main__':
  main()
