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

"""
Helper functions that are shared amongst prereqs and build scripts across various
platforms.
"""

import distutils.spawn
import platform
import shutil
import subprocess
import os
import urllib.request

def run_command(cmd, capture_output=False, cwd=None, check=False, as_root=False):
 """Run a command.

 Args:
  cmd (list(str)): Command to run as a list object.
          Eg: ['ls', '-l'].
  capture_output (bool): Capture the output of this command.
                         Output can be accessed as <return_object>.stdout
  cwd (str): Directory to execute the command from.
  check (bool): Raises a CalledProcessError if True and the command errored out
  as_root (bool): Run command as root user with admin priveleges (supported on mac and linux).

 Raises:
  (subprocess.CalledProcessError): If command errored out and `text=True`

 Returns:
  (`subprocess.CompletedProcess`): object containing information from command execution
 """

 if as_root and (is_mac_os() or is_linux_os()):
  cmd.insert(0, 'sudo')

 cmd_string = ' '.join(cmd)
 print('Running cmd: {0}\n'.format(cmd_string))
 # If capture_output is requested, we also set text=True to store the returned value of the
 # command as a string instead of bytes object
 return subprocess.run(cmd, capture_output=capture_output, cwd=cwd, check=check,
                       text=capture_output)


def is_command_installed(tool):
  """Check if a command is installed on the system."""
  return distutils.spawn.find_executable(tool)


def delete_directory(dir_path):
  """Recursively delete a valid directory"""
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)


def download_file(url, file_path):
  """Download from url and save to specified file path."""
  with urllib.request.urlopen(url) as response, open(file_path, 'wb') as out_file:
    shutil.copyfileobj(response, out_file)


def unpack_files(archive_file_path, output_dir=None):
  """Unpack/extract an archive to specified output_directory"""
  shutil.unpack_archive(archive_file_path, output_dir)


def is_windows_os():
 return platform.system() == 'Windows'


def is_mac_os():
 return platform.system() == 'Darwin'


def is_linux_os():
 return platform.system() == 'Linux'


def check_vcpkg_triplet(triplet_name, arch, crt_linkage):
  """Check if a vcpkg triplet exists that match our specification.

  Args:
    arch (str): Architecture (eg: 'x86', 'x64').
    crt_linkage (str): Runtime linkage for MSVC (eg: 'dynamic', 'static')

  Returns:
    (bool): If the triplet is valid.
  """
  triplet_file_path = get_vcpkg_triplet_file_path(triplet_name)
  if not os.path.exists(triplet_file_path):
    return False

  _arch = None
  _crt_linkage = None

  with open(triplet_file_path, 'r') as triplet_file:
    for line in triplet_file:
      print("Orginal: {0}".format(line))
      # Eg: set(VCPKG_TARGET_ARCHITECTURE x86) ->
      #     set(VCPKG_TARGET_ARCHITECTURE x86
      line = line.rstrip('\n').rstrip(')')
      if not line.startswith('set('):
        continue
      print(line)
      # Eg: 'set(', 'VCPKG_TARGET_ARCHITECTURE x86'
      variable = line.split('set(')[-1]
      print(variable)
      variable_name, variable_value = variable.split(' ')
      if variable_name == 'VCPKG_TARGET_ARCHITECTURE':
        _arch = variable_value
      elif variable_name == 'VCPKG_CRT_LINKAGE':
        _crt_linkage = variable_value

  return (_arch == arch) and (_crt_linkage == crt_linkage)


def create_vcpkg_triplet(arch, crt_linkage):
  """Create a new triplet configuration.

  Args:
    arch (str): Architecture (eg: 'x86', 'x64').
    crt_linkage (str): Runtime linkage for MSVC (eg: 'dynamic', 'static')

  Returns:
    (str): Triplet name
      Eg: 'x86-windows-dynamic'
  """
  contents = [ 'VCPKG_TARGET_ARCHITECTURE {0}'.format(arch),
               'VCPKG_CRT_LINKAGE {0}'.format(crt_linkage),
               'VCPKG_LIBRARY_LINKAGE static']

  contents = ['set({0})\n'.format(content) for content in contents]
  if is_linux_os() or is_mac_os():
    contents.append('\n')
    contents.append('set(VCPKG_CMAKE_SYSTEM_NAME {0})\n'.format(platform.system()))

  triplet_name = '{0}-{1}-{2}'.format(arch.lower(), platform.system().lower(), crt_linkage)
  with open(get_vcpkg_triplet_file_path(triplet_name), 'w') as triplet_file:
    triplet_file.writelines(contents)
    print('Created new triplet: {0}'.format(triplet_name))

  return triplet_name


def get_vcpkg_triplet(arch='x64', crt_linkage='dynamic'):
  """Get vcpkg target triplet (platform definition).

  Args:
    arch (str): Architecture (eg: 'x86', 'x64').
    crt_linkage (str): C runtime library linkage for MSVC (eg: 'dynamic', 'static')

  Raises:
    ValueError: If current OS is not win,mac or linux.

  Returns:
    (str): Triplet name.
       Eg: "x64-windows-static".
  """
  triplet_name = [arch]
  if is_windows_os():
    triplet_name.append('windows')
    # For windows, default is to build dynamic. Hence we specify static.
    # For mac/linux, default is to build static libraries
    triplet_name.append(crt_linkage)
  elif is_mac_os():
    triplet_name.append('osx')
  elif is_linux_os():
    triplet_name.append('linux')
    if arch == 'x86':  # special case for x86-linux-dynamic
     triplet_name.append('dynamic')

  triplet_name = '-'.join(triplet_name)
  ok = check_vcpkg_triplet(triplet_name, arch, crt_linkage)
  if not ok:
    print("noooooooooo")
    triplet_name = create_vcpkg_triplet(arch, crt_linkage)

  print("Using vcpkg triplet: {0}".format(triplet_name))
  return triplet_name


def get_vcpkg_triplet_file_path(triplet_name):
  """Get absolute path to vcpkg triplet configuration file."""
  return os.path.join(get_vcpkg_root_path(), 'triplets',
                                   triplet_name + '.cmake')


def get_vcpkg_response_file_path(triplet_name):
  """Get absolute path to vcpkg response file containing packages to install"""
  response_file_dir_path = os.path.join(os.getcwd(), 'external')
  response_file_path = os.path.join(response_file_dir_path,
                       'vcpkg_' + triplet_name + '_response_file.txt')
  # The firebase-cpp-sdk repo ships with pre-created response files
  # (list of packages to install + a triplet name at the end)
  # for common triplets supported by a fresh vcpkg install.
  # These response files are located at <repo_root>/external/vcpkg_*
  # If we do not find a matching response file to specified triplet,
  # we have to create a new file. In order to do this, we copy any of
  # the existing files (vcpkg_x64-linux_response_file.txt in this case)
  # and modify the triplet name contained in it. Copying makes sure that the
  # list of packages to install is the same as other response files.
  if not os.path.exists(response_file_path):
    existing_response_file_path = os.path.join(response_file_dir_path,
                         'vcpkg_x64-linux_response_file.txt')
    with open(existing_response_file_path, 'r') as existing_file:
      # Structure of a vcpkg response file
      # <package1>
      # <package2>
      # ...
      # --triplet
      # <triplet_name>
      lines = existing_file.readlines()
      print("Using x64-linux response file as template.\n")
      print(lines)
      # Modify the line containing triplet name
      lines[-1] = triplet_name + '\n'
      with open(response_file_path, 'w') as response_file:
        response_file.writelines(lines)
        print("Created new response file: {0}\n".format(response_file_path))
        print(lines)
  return response_file_path


def get_vcpkg_root_path():
  """Get absolute path to vcpkg root directory in repo."""
  return os.path.join(os.getcwd(), 'external', 'vcpkg')


def get_vcpkg_executable_file_path():
  """Get absolute path to vcpkg executable."""
  vcpkg_root_dir = get_vcpkg_root_path()
  if is_windows_os():
    vcpkg_executable_file_path = os.path.join(vcpkg_root_dir, 'vcpkg.exe')
  elif is_linux_os() or is_mac_os():
    vcpkg_executable_file_path = os.path.join(vcpkg_root_dir, 'vcpkg')
  return vcpkg_executable_file_path


def get_vcpkg_installation_script_path():
  """Get absolute path to the script used to build and install vcpkg."""
  vcpkg_root_dir = get_vcpkg_root_path()
  if is_windows_os():
    script_absolute_path = os.path.join(vcpkg_root_dir, 'bootstrap-vcpkg.bat')
  elif is_linux_os() or is_mac_os():
    script_absolute_path = os.path.join(vcpkg_root_dir, 'bootstrap-vcpkg.sh')

  return script_absolute_path


def get_vcpkg_cmake_toolchain_file_path():
  """Get absolute path to toolchain file used for cmake builds"""
  vcpkg_root_dir = get_vcpkg_root_path()
  return os.path.join(vcpkg_root_dir, 'scripts', 'buildsystems', 'vcpkg.cmake')


def clean_vcpkg_temp_data():
  """Delete files/directories that vcpkg uses during its build"""
  # Clear temporary directories and files created by vcpkg buildtrees
  # could be several GBs and cause github runners to run out of space
  vcpkg_root_dir_path = get_vcpkg_root_path()
  buildtrees_dir_path = os.path.join(vcpkg_root_dir_path, 'buildtrees')
  delete_directory(buildtrees_dir_path)
  downloads_dir_path = os.path.join(vcpkg_root_dir_path, 'downloads')
  delete_directory(downloads_dir_path)
