#!/usr/bin/env python

# Copyright 2019 Google
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
Use this script to configure and build the firebase cpp sdk
If you see any issues in configure when run multiple times, 
try deleting your build directory and use this script after that.

Usage:
# Build all targets with default options
python build.py

# Build all targets with default options and also build unit tests
python build.py --build_tests

# Build all targets without using vcpkg
# Assuming cpp dependencies are installed and accessible to the build system
python build.py --no_vcpkg

"""

import subprocess
import os
import sys
import multiprocessing
import argparse


def run_command(cmd, cwd=None, as_root_user=False, collect_output=False):
    """Run a command

    Args:
        cmd (:obj:`list` of :obj:`str`): Command to run as a list object
                                         Eg: ['ls', '-l']
        cwd (str): Directory to execute the command from
        as_root_user (bool): Run command as root user with admin priveleges (supported on mac and linux)
        collect_output (bool): Get output from the command. The string returned from this function
                               call will have the output from command execution.
    Raises:
        (IOError): If command errored out
    Returns:
        (str|None): If collect_output is provided, a string is returned
                    None if output is not requested
    """
    if as_root_user and (is_mac_os() or is_linux_os()):
        cmd.insert(0, 'sudo')

    cmd_string = ' '.join(cmd)
    print('Running cmd: {0}\n'.format(cmd_string))
    try:
        if collect_output:
            output = subprocess.check_output(cmd, cwd=cwd)
            return output.decode('utf-8').strip()
        else:
            subprocess.call(cmd, cwd=cwd)
    except subprocess.CalledProcessError:
        raise IOError('Error executing {0}'.format(cmd_string))


def get_repo_root_dir():
    """ Get root directory of current git repo

    This function takes the guesswork out when executing commands
    or locating files relative to root of the repo

    Raises:
        IOError: Current directory is not in a git repo

    Returns:
        (str): Root directory of git repo
    """
    return run_command(['git', 'rev-parse', '--show-toplevel'], collect_output=True)


REPO_ROOT_DIR = get_repo_root_dir()

def is_windows_os():
    """Check if the current OS is Windows

    Returns:
        (bool): True if windows, False otherwise
    """
    return sys.platform.startswith('win')


def is_linux_os():
    """Check if the current OS is Linux

    Returns:
        (bool): True if windows, False otherwise
    """
    return sys.platform.startswith('linux')


def is_mac_os():
    """Check if the current OS is mac

    Returns:
        (bool): True if windows, False otherwise
    """
    return sys.platform.startswith('darwin')


def get_vcpkg_triplet(arch):
    """ Get vcpkg target triplet (platform definition)

    Args:
        arch (str): Architecture (eg: 'x86', 'x64')

    Raises:
        ValueError: If current OS is not win,mac or linux
    
    Returns:
        (str): Triplet name
               Eg: "x64-windows-static"
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
    else:
        raise ValueError('Unsupported operating system \
                          We currently support windows, mac, linux')

    return '-'.join(triplet_name)


def step_cmake_configure(args):
    """ CMake configure

    If you are seeing problems when running this multiple times,
    make sure to clean/delete previous build directory

    Args:
        args (`argparse.Namespace`): Parsed command line arguments
    """   
    cmd = ['cmake', '-S', '.', '-B', args.build_dir]
    
    # If generator is not specifed, default for platform is used by cmake, else 
    # use the specified value
    if args.generator:
        cmd.extend(['-G', args.generator])
    if args.config:
        cmd.append('-DCMAKE_BUILD_TYPE={0}'.format(args.config))
    if args.build_tests:
        cmd.append('-DFIREBASE_CPP_BUILD_TESTS=ON')
        cmd.append('-DFIREBASE_FORCE_FAKE_SECURE_STORAGE=ON')
    # If you want to avoid vcpkg altogether and have cpp dependencies installed elsewhere
    if not args.no_vcpkg:
        vcpkg_toolchain_file_path = os.path.join(REPO_ROOT_DIR, 'external', 'vcpkg', 
                                                'scripts', 'buildsystems', 'vcpkg.cmake')
        
        cmd.append('-DCMAKE_TOOLCHAIN_FILE={0}'.format(vcpkg_toolchain_file_path))
        cmd.append('-DVCPKG_TARGET_TRIPLET={0}'.format(get_vcpkg_triplet(args.arch)))
    
    cmd.append('-DFIREBASE_INCLUDE_FIRESTORE=OFF')
    run_command(cmd, cwd=REPO_ROOT_DIR)


def step_cmake_build(args):
    """ CMake build

    Args:
        args (`argparse.Namespace`): Parsed command line arguments
    """   
    cmd = ['cmake', '--build', args.build_dir, '-j', str(args.j)]
    if args.target:
        targets = ['--target']
        targets.extend(args.target)
        cmd.extend(targets)
    run_command(cmd, cwd=REPO_ROOT_DIR)


def main():
    args = parse_cmdline_args()

    step_cmake_configure(args)
    step_cmake_build(args)


def parse_cmdline_args():
    parser = argparse.ArgumentParser(description='Install Prerequisites for building cpp sdk')
    parser.add_argument('-A', '--arch', default='x64', help='Platform architecture (x64, x86)')
    parser.add_argument('-G', '--generator', help='Build generator to use')
    parser.add_argument('--build_dir', default='build', help='Output build directory')
    parser.add_argument('--build_tests', action='store_true', help='Build unit tests too')
    parser.add_argument('--no_vcpkg', action='store_true', help='Do not use vcpkg toolchain')
    parser.add_argument('--config', help='Release/Debug config')
    parser.add_argument('--target', nargs='+', help='A list of CMake build targets (eg: firebase_app firebase_auth)')
    parser.add_argument('-j', default=multiprocessing.cpu_count(), help='Number of CPU cores to use for build (default=max)')
    args = parser.parse_args()
    return args

if __name__ == '__main__':
    main()