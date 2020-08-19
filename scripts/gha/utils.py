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
 return subprocess.run(cmd, capture_output=capture_output, cwd=cwd, check=check, text=capture_output)


def is_command_installed(tool):
  """Check if a command is installed on the system."""
  return distutils.spawn.find_executable(tool)


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


def get_vcpkg_triplet(arch):
  """ Get vcpkg target triplet (platform definition).

  Args:
    arch (str): Architecture (eg: 'x86', 'x64').

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
    triplet_name.append('static')
  elif is_mac_os():
    triplet_name.append('osx')
  elif is_linux_os():
    triplet_name.append('linux')

  triplet_name = '-'.join(triplet_name)
  print("Using vcpkg triplet: {0}".format(triplet_name))
  return triplet_name


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
