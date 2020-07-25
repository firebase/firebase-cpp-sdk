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
This script needs to be run once to prepare the machine for building SDK.
It will download required python dependencies and also install ccache on mac/linux.
ccache considerably improves the build times.

Run this script from the root of the repository

Usage:
python scripts/install_prereqs.py

"""

import subprocess
import distutils.spawn
import platform
import urllib.request
import shutil


def run_command(cmd, cwd=None, as_root_user=False, collect_output=False):
  """Run a command.

  Args:
    cmd (list(str)): Command to run as a list object.
                    Eg: ['ls', '-l'].
    cwd (str): Directory to execute the command from.
    as_root_user (bool): Run command as root user with admin priveleges (supported on mac and linux).
    collect_output (bool): Get output from the command. The string returned from this function
               call will have the output from command execution.
  Raises:
    (subprocess.CalledProcessError): If command errored out.
  Returns:
    (str|None): If collect_output is provided, a string is returned
          None if output is not requested.
  """
  if as_root_user and (is_mac_os() or is_linux_os()):
    cmd.insert(0, 'sudo')

  cmd_string = ' '.join(cmd)
  print('Running cmd: {0}\n'.format(cmd_string))
  if collect_output:
    output = subprocess.check_output(cmd, cwd=cwd)
    return output.decode('utf-8').strip()
  else:
    subprocess.call(cmd, cwd=cwd)


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


def main():
  # Install ccache on linux/mac
  if not is_command_installed('ccache'):
    if platform.system() == 'Linux':
        cmd = ['apt', 'install', 'ccache']
        run_command(cmd, as_root_user=True)

    elif platform.system() == 'Darwin':
        cmd = ['brew', 'install', 'ccache']
        run_command(cmd)

  # Install required python dependencies
  python3_cmd = 'python3' if is_command_installed('python3') else 'python'
  cmd = [python3_cmd, '-m', 'pip', 'install', '-r', 'external/pip_requirements.txt', '--user']
  run_command(cmd)


if __name__ == '__main__':
  main()
