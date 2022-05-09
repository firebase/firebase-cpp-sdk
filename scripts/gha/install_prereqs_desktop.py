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

Please note that this script is aganostic of various desktop configurations.
For example, you can run it once regardless if you are following up with a build of x86 or x64.

Run this script from the root of the repository

Usage:
python scripts/gha/install_prereqs_desktop.py

"""

import argparse
import utils

def main():
  args = parse_cmdline_args()

  if not args.running_only:
    # Install protobuf on linux/mac if its not installed already
    if not utils.is_command_installed('protoc'):
      if utils.is_linux_os():
          # sudo apt install protobuf-compiler
          utils.run_command(['apt', 'install', '-y','protobuf-compiler'], as_root=True)
      elif utils.is_mac_os():
          # brew install protobuf
          utils.run_command(['brew', 'install', 'protobuf'])

    # Install go on linux/mac if its not installed already
    if not utils.is_command_installed('go'):
      if utils.is_linux_os():
          # sudo apt install -y golang
          utils.run_command(['apt', 'install', '-y','golang'], as_root=True)
      elif utils.is_mac_os():
          # brew install go
          utils.run_command(['brew', 'install', 'go'])

    # Install openssl on linux/mac if its not installed already
    if not utils.is_command_installed('openssl'):
      if utils.is_linux_os():
          # sudo apt install -y openssl
          utils.run_command(['apt', 'install', '-y','openssl'], as_root=True)
      elif utils.is_mac_os():
          # brew install openssl
          utils.run_command(['brew', 'install', 'openssl'])

    # Install ccache on linux/mac if its not installed already
    if not utils.is_command_installed('ccache'):
      if utils.is_linux_os():
          # sudo apt install ccache
          utils.run_command(['apt', 'install', '-y', 'ccache'], as_root=True)
      elif utils.is_mac_os():
          # brew install ccache
          utils.run_command(['brew', 'install', 'ccache'])

    # Install clang-format on linux/mac if its not installed already
    if not utils.is_command_installed('clang-format'):
      if utils.is_linux_os():
          # sudo apt install clang-format
          utils.run_command(['apt', 'install', '-y','clang-format'], as_root=True)
      elif utils.is_mac_os():
          # brew install protobuf
          utils.run_command(['brew', 'install', 'clang-format'])

    # Install required python dependencies.
    # On Catalina, python2 in installed as default python.
    # Example command:
    # python3 -m pip install -r external/pip_requirements.txt --user
    utils.run_command(
       ['python3' if utils.is_command_installed('python3') else 'python', '-m',
            'pip', 'install', '-r', 'external/pip_requirements.txt', '--user'] )

  if args.arch == 'x86':
    utils.install_x86_support_libraries(args.gha_build)

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Install prerequisites for building cpp sdk')
  parser.add_argument('--arch', default=None, help='Install support libraries to build a specific architecture (currently supported: x86)')
  parser.add_argument('--running_only', action='store_true', help='Only install prerequisites for running, not for building')
  parser.add_argument('--gha_build', action='store_true', default=None, help='Set to true when building on GitHub, changing some prerequisite installation behavior.')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()
