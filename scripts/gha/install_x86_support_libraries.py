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
Install Linux x86 support libraries.
"""

import argparse
import os
import utils
import shutil
import subprocess
import sys


def install(gha_build=False):
  """Install support libraries needed to build x86 on x86_64 hosts.

  Args:
    gha_build: Pass in True if running on a GitHub runner; this will activate
               workarounds that might be undesirable on a personal system (e.g.
               downgrading Ubuntu packages).
  """
  if utils.is_linux_os():
    packages = ['gcc-multilib', 'g++-multilib', 'libglib2.0-dev:i386',
                'libsecret-1-dev:i386', 'libpthread-stubs0-dev:i386',
                'libssl-dev:i386']
    if gha_build:
      # Workaround for GitHub runners, which have an incompatibility between the
      # 64-bit and 32-bit versions of the Ubuntu package libpcre2-8-0. Downgrade
      # the installed 64-bit version of the library to get around this issue.
      # This will presumably be fixed in a future Ubuntu update. (If you remove
      # it, remove the workaround further down this function as well.)
      packages = ['--allow-downgrades'] + packages + ['libpcre2-8-0=10.34-7']

    # First check if these packages exist on the machine already
    devnull = open(os.devnull, "w")
    process = subprocess.run(["dpkg", "-s"] + packages, stdout=devnull, stderr=subprocess.STDOUT)
    devnull.close()
    if process.returncode != 0:
      # This implies not all of the required packages are already installed on user's machine
      # Install them.
      utils.run_command(['dpkg', '--add-architecture', 'i386'], as_root=True, check=True)
      utils.run_command(['apt', 'update'], as_root=True, check=True)
      utils.run_command(['apt', 'install', '-V', '-y'] + packages, as_root=True, check=True)

    if gha_build:
      # One more workaround: downgrading libpcre2-8-0 above may have uninstalled
      # libsecret, which is required for the Linux build. Force it to be
      # reinstalled, but do it as a separate command to ensure that held
      # packages aren't modified. (Once the workaround above is removed, this can
      # be removed as well.)
      # Note: "-f" = "fix" - let apt do what it needs to do to fix dependencies.
      utils.run_command(['apt', 'install', '-f', '-V', '-y', 'libsecret-1-dev'],
                        as_root=True, check=True)


def main():
  args = parse_cmdline_args()
  if not utils.is_linux_os():
      print("This tool is for Linux only.")
      exit(1)
  install(args.gha_build)


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Install Linux x86 support packages')
  parser.add_argument('--gha_build', action='store_true', default=None, help='Set to true when building on GitHub. Changes some prerequisite installation behavior to GitHub-specific workarounds.')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()
