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

import os
import argparse
from print_matrix_configuration import PARAMETERS
import utils

def main():
  args = parse_cmdline_args()

  for k, v in os.environ.items():
      print(f'{k}={v}')

  # Forces all git commands to use authenticated https, to prevent throttling.
  utils.run_command("git config --global credential.helper 'store --file /tmp/git-credentials'")
  utils.run_command("echo 'https://%s@github.com' > /tmp/git-credentials" % os.getenv('GITHUB_TOKEN'))

  # setup Xcode version for macOS, iOS
  if args.platform == 'iOS' or (args.platform == 'Desktop' and utils.is_mac_os()):
    xcode_version = PARAMETERS['integration_tests']['matrix']['xcode_version'][0]
    utils.run_command('sudo xcode-select -s /Applications/Xcode_$%s.app/Contents/Developer' % xcode_version)

 # This prevents errors arising from the shut down of binutils, used by older version of homebrew for hosting packages.
  if utils.is_mac_os():
    utils.run_command('brew update')

  if args.platform == 'Desktop':
    # Set env vars
    if utils.is_linux_os():
      os.environ['VCPKG_TRIPLET'] = 'x64-linux'
      utils.run_command('echo "VCPKG_TRIPLET=x64-linux" ">> $GITHUB_ENV')
    elif utils.is_mac_os():
      utils.run_command('echo', 'VCPKG_TRIPLET=x64-osx', '>>', '$GITHUB_ENV')
    elif utils.is_windows_os():
      os.environ['VCPKG_TRIPLET'] = 'x64-windows-static'
      utils.run_command('echo "VCPKG_TRIPLET=x64-windows-static" >> $GITHUB_ENV')
      # Enable Git Long-paths Support
      utils.run_command('git config --system core.longpaths true')
    os.environ['VCPKG_RESPONSE_FILE'] = 'external/vcpkg_$%s_response_file.txt' % os.getenv('VCPKG_TRIPLET')
    utils.run_command('echo "VCPKG_RESPONSE_FILE=external/vcpkg_$%s_response_file.txt" >> $GITHUB_ENV' % os.getenv('VCPKG_TRIPLET'))

    # Install openssl on linux/mac if its not installed already
    if args.ssl == 'openssl' and not utils.is_command_installed('openssl'):
      if utils.is_linux_os():
        # sudo apt install -y openssl
        utils.run_command('sudo apt install -y openssl')
      elif utils.is_mac_os():
        # brew install openssl
        utils.run_command('brew install openssl')
        utils.run_command('echo "OPENSSL_ROOT_DIR=/usr/local/opt/openssl@1.1" >> $GITHUB_ENV')
      elif utils.is_windows_os():
        utils.run_command(['choco install openssl -r'])

  for k, v in os.environ.items():
      print(f'{k}={v}')

  if not args.running_only:
    # Install go on linux/mac if its not installed already
    if not utils.is_command_installed('go'):
      if utils.is_linux_os():
          # sudo apt install -y golang
          utils.run_command('sudo apt install -y golang')
      elif utils.is_mac_os():
          # brew install go
          utils.run_command('brew install go')

    # Install ccache on linux/mac if its not installed already
    if not utils.is_command_installed('ccache'):
      if utils.is_linux_os():
          # sudo apt install ccache
          utils.run_command('sudo apt install -y ccache')
      elif utils.is_mac_os():
          # brew install ccache
          utils.run_command('brew install ccache')

    # Install clang-format on linux/mac if its not installed already
    if not utils.is_command_installed('clang-format'):
      if utils.is_linux_os():
          # sudo apt install clang-format
          utils.run_command('sudo apt install -y clang-format')
      elif utils.is_mac_os():
          # brew install protobuf
          utils.run_command('brew install clang-format')

  if args.arch == 'x86':
    utils.install_x86_support_libraries(args.gha_build)

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Install prerequisites for building cpp sdk')
  parser.add_argument('--platform', default='Desktop', help='Install prereqs for certain platform')
  parser.add_argument('--arch', default=None, help='Install support libraries to build a specific architecture (currently supported: x86)')
  parser.add_argument('--running_only', action='store_true', help='Only install prerequisites for running, not for building')
  parser.add_argument('--gha_build', action='store_true', default=None, help='Set this option when building on GitHub, changing some prerequisite installation behavior')
  parser.add_argument('--ssl', default='openssl', help='Which SSL is this build using (supported: openssl, boringssl)')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()
