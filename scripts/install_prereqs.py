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


def is_tool_installed(tool):
  """Check if a tool is installed on the system.

  Args:
    tool (str): Name of the tool.

  Returns:
    (bool) : True if installed on the system, False otherwise.
  """
  return distutils.spawn.find_executable(tool)


def main():
  # Install ccache on linux/mac
  if platform.system() == 'Linux':
   if not is_tool_installed('apt'):
    print('Skipping ccache install because apt is not found')
   else:
    cmd = ['apt', 'install', 'ccache']
    run_cmd(cmd, as_root_user=True)

  elif platform.system() == 'Darwin':
   if not is_tool_installed('brew'):
    print('Skipping ccache install because brew is not found')
   else:
    cmd = ['brew', 'install', 'ccache']
    run_cmd(cmd, as_root_user=True)

  # Install required python dependencies
  cmd = ['python', '-m', 'pip', 'install', '-r', 'external/pip_requirements.txt', '--user']
  run_command(cmd)


if __name__ == '__main__':
  main()
