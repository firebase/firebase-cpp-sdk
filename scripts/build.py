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
Assuming pre-requisites are in place (please run `scripts/install_preqreqs.py` once on the system), 
this script will build the firebase cpp sdk

It does the following,
 - install platform specific cpp dependencies via vcpkg
 - cmake configure
 - cmake build

Usage examples:
# Build all targets with x64 architecture, Release config and 8 cpu cores fo
python build.py --arch x64 --config Release -j 8

# Build all targets with default options and also build unit tests
python build.py --build_tests --arch x64

# Build only firebase_app and firebase_auth with default options and also build unit tests
python build.py --build_tests --arch x64 --targets firebase_app firebase_auth

"""

import subprocess
import os
import sys
import argparse
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


def get_vcpkg_executable_file_path():
    """Get absolute path to vcpkg executable."""
    vcpkg_root_dir = os.path.join(os.getcwd(), 'external', 'vcpkg')
    if is_windows_os():
        vcpkg_executable_file_path = os.path.join(vcpkg_root_dir, 'vcpkg.exe')
    elif is_linux_os() or is_mac_os():
        vcpkg_executable_file_path = os.path.join(vcpkg_root_dir, 'vcpkg')
    return vcpkg_executable_file_path


def get_vcpkg_installation_script_path():
    """Get absolute path to the script used to build and install vcpkg."""
    vcpkg_root_dir = os.path.join(os.getcwd(), 'external', 'vcpkg')
    if is_windows_os():
        script_absolute_path = os.path.join(vcpkg_root_dir, 'bootstrap-vcpkg.bat')
    elif is_linux_os() or is_mac_os():
        script_absolute_path = os.path.join(vcpkg_root_dir, 'bootstrap-vcpkg.sh')

    return script_absolute_path



def install_cpp_dependencies_with_vcpkg(arch):
    """Install packages with vcpkg.

    This does the following,
        - initialize/update the vcpkg git submodule
        - build vcpkg executable
        - install packages via vcpkg.
    Args:
        arch (str): Architecture (eg: 'x86', 'x64').
    """
    # vcpkg is a submodule of the main cpp repo
    # Ensure that the submodule is initialized and updated
    run_command(['git', 'submodule', 'init'])
    run_command(['git', 'submodule', 'update'])

    # Install vcpkg executable if its not installed already
    vcpkg_executable_file_path = get_vcpkg_executable_file_path()
    found_vcpkg_executable = os.path.exists(vcpkg_executable_file_path)
    if not found_vcpkg_executable:
        script_absolute_path = get_vcpkg_installation_script_path()
        # mac example: ./external/vcpkg/bootstrap-sh
        run_command([script_absolute_path])

    # for each desktop platform, there is an existing vcpkg response file 
    # in the repo (external/vcpkg_<triplet>_response_file.txt) defined for each target triplet
    vcpkg_triplet = get_vcpkg_triplet(arch)
    vcpkg_response_file_path = os.path.join(os.getcwd(), 'external', 
                                            'vcpkg_' + vcpkg_triplet + '_response_file.txt')

    # mac example: ./external/vcpkg/vcpkg install @external/vcpkg_x64-osx_response_file.txt --disable-metrics
    run_command([vcpkg_executable_file_path, 'install', '@' + vcpkg_response_file_path, '--disable-metrics'])


def step_cmake_configure(build_dir, arch, build_tests=True, generator=None, config=None):
    """ CMake configure.

    If you are seeing problems when running this multiple times,
    make sure to clean/delete previous build directory.

    Args:
      build_dir (str): Output build directory.
      arch (str): Platform Architecture (example: 'x64').
      build_tests (bool): Build cpp unit tests.
      generator (str): Valid CMake generator (example: 'Unix Makefiles').
                       If its not specified, cmake determines default generator based on platform.
      config (str): Release/Debug config.
                    If its not specified, cmake's default is used (most likely Debug).
    """   
    cmd = ['cmake', '-S', '.', '-B', build_dir]
    
    # If generator is not specifed, default for platform is used by cmake, else 
    # use the specified value
    if generator:
        cmd.extend(['-G', generator])
    if config:
        cmd.append('-DCMAKE_BUILD_TYPE={0}'.format(config))
    if build_tests:
        cmd.append('-DFIREBASE_CPP_BUILD_TESTS=ON')
        cmd.append('-DFIREBASE_FORCE_FAKE_SECURE_STORAGE=ON')

    vcpkg_toolchain_file_path = os.path.join(os.getcwd(), 'external', 'vcpkg', 'scripts', 'buildsystems', 'vcpkg.cmake')
    cmd.append('-DCMAKE_TOOLCHAIN_FILE={0}'.format(vcpkg_toolchain_file_path))

    vcpkg_triplet = get_vcpkg_triplet(arch)
    cmd.append('-DVCPKG_TARGET_TRIPLET={0}'.format(vcpkg_triplet))
    
    # TODO: Remove this once firestore is included in the build and everything works
    cmd.append('-DFIREBASE_INCLUDE_FIRESTORE=OFF')
    run_command(cmd)


def step_cmake_build(build_dir, cpu_cores, targets=None):
    """ CMake build.

    Args:
      build_dir (str): Output build directory.
      cpu_cores (str): Number of cpu cores.
      targets (list(str)): Specific build targets.
                          If not specified, cmake will build all targets.
    """   
    cmd = ['cmake', '--build', build_dir, '-j', cpu_cores]
    if targets:
        targets_cmd_line_arg = ['--target']
        targets_cmd_line_arg.extend(args.target)
        cmd.extend(targets)

    run_command(cmd)


def main():
    args = parse_cmdline_args()
    
    # Install platform dependent cpp dependencies with vcpkg 
    install_cpp_dependencies_with_vcpkg(args.arch)
    # CMake configure
    step_cmake_configure(args.build_dir, args.build_test, args.generator, args.config, )
    # CMake build
    step_cmake_build(args.build_dir, args.cpu_cores, args.target)


def parse_cmdline_args():
    parser = argparse.ArgumentParser(description='Install Prerequisites for building cpp sdk')
    parser.add_argument('-a', '--arch', default='x64', help='Platform architecture (x64, x86)')
    parser.add_argument('-G', '--generator', help='Build generator to use')
    parser.add_argument('--build_dir', default='build', help='Output build directory')
    parser.add_argument('--build_tests', action='store_true', help='Build unit tests too')
    parser.add_argument('--config', help='Release/Debug config')
    parser.add_argument('--target', nargs='+', help='A list of CMake build targets (eg: firebase_app firebase_auth)')
    parser.add_argument('-j', default=str(os.cpu_count()), help='Number of CPU cores to use for build (default=max)')
    args = parser.parse_args()
    return args

if __name__ == '__main__':
    main()

