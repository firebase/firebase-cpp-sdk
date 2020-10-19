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
Assuming pre-requisites are in place (from running 
`scripts/gha/install_prereqs_desktop.py`), this builds the firebase cpp sdk.

It does the following,
- install desktop platform specific cpp dependencies via vcpkg
- cmake configure
- cmake build

Usage examples:
# Build all targets with x64 architecture, Release config and 8 cpu cores
python scripts/gha/build_desktop.py --arch x64 --config Release -j 8

# Build all targets with default options and also build unit tests
python scripts/gha/build_desktop.py --build_tests --arch x64

# Build only firebase_app and firebase_auth 
python scripts/gha/build_desktop.py --target firebase_app firebase_auth

"""

import argparse
import os
import utils
import shutil
import subprocess

def append_line_to_file(path, line):
  """Append the given line to the end of the file if it's not already in the file.

  Used to disable specific errors in CMakeLists files if needed.

  Args:
    path: File to edit.
    line: String to add to the end of the file.
  """
  with open(path, "r") as file:
    lines = file.readlines()
  if line not in lines:
    with open(path, "a") as file:
      file.write("\n" + line + "\n")

def install_cpp_dependencies_with_vcpkg(arch):
  """Install packages with vcpkg.

  This does the following,
    - initialize/update the vcpkg git submodule
    - build vcpkg executable
    - install packages via vcpkg.
  Args:
    arch (str): Architecture (eg: 'x86', 'x64').
  """

  # Install vcpkg executable if its not installed already
  vcpkg_executable_file_path = utils.get_vcpkg_executable_file_path()
  found_vcpkg_executable = os.path.exists(vcpkg_executable_file_path)
  if not found_vcpkg_executable:
    script_absolute_path = utils.get_vcpkg_installation_script_path()
    # Example: ./external/vcpkg/bootstrap-sh
    utils.run_command([script_absolute_path])

  # for each desktop platform, there exists a vcpkg response file in the repo
  # (external/vcpkg_<triplet>_response_file.txt) defined for each target triplet
  vcpkg_triplet = utils.get_vcpkg_triplet(arch)
  vcpkg_response_file_path = os.path.join(os.getcwd(), 'external', 
                      'vcpkg_' + vcpkg_triplet + '_response_file.txt')

  # Eg: ./external/vcpkg/vcpkg install @external/vcpkg_x64-osx_response_file.txt 
  # --disable-metrics
  utils.run_command([vcpkg_executable_file_path, 'install',
                     '@' + vcpkg_response_file_path, '--disable-metrics'])

  vcpkg_root_dir_path = utils.get_vcpkg_root_path()

  # Clear temporary directories and files created by vcpkg buildtrees
  # could be several GBs and cause github runners to run out of space
  utils.clean_vcpkg_temp_data()


def cmake_configure(build_dir, arch,
                    build_tests=True, config=None, target_format=None):
  """ CMake configure.

  If you are seeing problems when running this multiple times,
  make sure to clean/delete previous build directory.

  Args:
   build_dir (str): Output build directory.
   arch (str): Platform Architecture (example: 'x64').
   build_tests (bool): Build cpp unit tests.
   config (str): Release/Debug config.
          If its not specified, cmake's default is used (most likely Debug).
   target_format (str): If specified, build for this targetformat ('frameworks' or 'libraries').
  """   
  cmd = ['cmake', '-S', '.', '-B', build_dir]
  
  # If generator is not specifed, default for platform is used by cmake, else 
  # use the specified value
  if config:
    cmd.append('-DCMAKE_BUILD_TYPE={0}'.format(config))
  if build_tests:
    cmd.append('-DFIREBASE_CPP_BUILD_TESTS=ON')
    cmd.append('-DFIREBASE_FORCE_FAKE_SECURE_STORAGE=ON')
  # temporary workaround for absl issue
  cmd.append('-DBUILD_TESTING=off')
  vcpkg_toolchain_file_path = os.path.join(os.getcwd(), 'external',
                                           'vcpkg', 'scripts', 
                                           'buildsystems', 'vcpkg.cmake')
  cmd.append('-DCMAKE_TOOLCHAIN_FILE={0}'.format(vcpkg_toolchain_file_path))

  vcpkg_triplet = utils.get_vcpkg_triplet(arch)
  cmd.append('-DVCPKG_TARGET_TRIPLET={0}'.format(vcpkg_triplet))
  if (target_format):
    cmd.append('-DTARGET_FORMAT={0})'.format(target_format))
  utils.run_command(cmd)

def main():
  args = parse_cmdline_args()
  
  # Ensure that the submodules are initialized and updated
  # Example: vcpkg is a submodule (external/vcpkg)
  utils.run_command(['git', 'submodule', 'init'])
  utils.run_command(['git', 'submodule', 'update'])

  # Install platform dependent cpp dependencies with vcpkg 
  install_cpp_dependencies_with_vcpkg(args.arch)

  # CMake configure
  cmake_configure(args.build_dir, args.arch,
                  args.build_tests, args.config, args.target_format)

  # Small workaround before build, turn off -Werror=sign-compare for a specific Firestore core lib.
  append_line_to_file(os.path.join(args.build_dir,
                                   'external/src/firestore/Firestore/core/CMakeLists.txt'),
                      'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=sign-compare")')

  # CMake build 
  # cmake --build build -j 8 
  cmd = ['cmake', '--build', args.build_dir, '-j', str(os.cpu_count()),
         '--config', args.config] 
  if args.target:
    # Example:  cmake --build build -j 8 --target firebase_app firebase_auth
    cmd.append('--target')
    cmd.extend(args.target)
  utils.run_command(cmd)
  # Copy libraries from appropriate vcpkg directory to build output
  # directory for later inclusion.
  vcpkg_path = ('external/vcpkg/installed/%s/%slib/' %
                (utils.get_vcpkg_triplet(args.arch),
                 'debug/' if args.config == 'Debug' else ''))
  if (os.path.exists(vcpkg_path)):
    shutil.rmtree('vcpkg-libs', ignore_errors=True)
    shutil.copytree(vcpkg_path, os.path.join(args.build_dir, 'vcpkg-libs'), )


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Install Prerequisites for building cpp sdk')
  parser.add_argument('-a', '--arch', default='x64', help='Platform architecture (x64, x86)')
  parser.add_argument('--build_dir', default='build', help='Output build directory')
  parser.add_argument('--build_tests', action='store_true', help='Build unit tests too')
  parser.add_argument('--config', help='Release/Debug config')
  parser.add_argument('--target', nargs='+', help='A list of CMake build targets (eg: firebase_app firebase_auth)')
  parser.add_argument('--target_format', default=None, help='(Mac only) whether to output frameworks (default) or libraries.')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()

